//
//  parser template
//  pomelo
//
//  Created by Edmund Kapusniak on 31/01/2018.
//  Copyright © 2018 Edmund Kapusniak. All rights reserved.
//


#include "$(header)"
#include <memory>



/*
    Parsing tables.
*/

const int START_STATE      = $(start_state);

const int ERROR_ACTION     = $(error_action);
const int ACCEPT_ACTION    = $(accept_action);

const int TOKEN_COUNT      = $(token_count);
const int NTERM_COUNT      = $(nterm_count);
const int STATE_COUNT      = $(state_count);
const int RULE_COUNT       = $(rule_count);
const int CONFLICT_COUNT   = $(conflict_count);

const unsigned short ACTION[] =
{
$(action_table)
};

const unsigned short CONFLICT[] =
{
$(conflict_table)
};

const unsigned short GOTO[] =
{
$(goto_table)
};

const struct { unsigned short nterm; unsigned short length; } RULE[] =
{
$(rule_table)
};



/*
    A type for productions that don't provide one.
*/

struct $(class_name)::empty
{
};



/*
    A value on the parser stack.  Can hold value of a token or of a production.
*/

class $(class_name)::value
{
public:

    value()                                 : _state( -1 ), _kind( -2 ) {}
    explicit value( int s )                 : _state( s ), _kind( -2 ) {}

    value( int s, const token_type& v )     : _state( s ), _kind( -1 ) { new ( (token_type*)_storage ) token_type( v ); }
    value( int s, $$(ntype_type)&& v )      : _state( s ), _kind( $$(ntype_value) ) { new ( ($$(ntype_type)*)_storage ) $$(ntype_type)( std::move( v ) ); }
    value( int s, const $$(ntype_type)& v ) : _state( s ), _kind( $$(ntype_value) ) { new ( ($$(ntype_type)*)_storage ) $$(ntype_type)( v ); }
    
    value( value&& v )                      : _state( v._state ) { construct( std::move( v ) ); }
    value( const value& v )                 : _state( v._state ) { construct( v ); }
    value& operator = ( value&& v )         { if ( &v != this ) { destroy(); _state = v._state; construct( std::move( v ) ); } return *this; }
    value& operator = ( const value& v )    { if ( &v != this ) { destroy(); _state = v._state; construct( v ); } return *this; }
    
    ~value()                                { destroy(); }
    
    int state()                             { return _state; }
    template < typename T > T& get()        { return *(T*)_storage; }
    template < typename T > T&& move()      { return std::move( *(T*)_storage; }
    
    
private:

    void construct( value&& v )
    {
        kind = v.kind;
        switch ( kind )
        {
        case -1: new ( (token_type*)storage ) token_type( v.move< token_type >() ); break;
        case $$(ntype_value): new ( ($$(ntype_type)*)storage ) $$(ntype_type)( v.move< $$(ntype_type) >() ); break;
        }
    }
    
    void construct( const value& v )
    {
        kind = v.kind;
        switch ( kind )
        {
        case -1: new ( (token_type*)storage ) token_type( v.get< token_type >() ); break;
        case $$(ntype_value): new ( ($$(ntype_type)*)storage ) $$(ntype_type)( v.get< $$(ntype_type) >() ); break;
        }
    }
    
    void destroy()
    {
        switch ( kind )
        {
        case -1: ( (token_type*)storage )->~token_type() ); break;
        case $$(ntype_value): ( ($$(ntype_type)*)storage )->~$$(ntype_type)(); break;
        }
    }

    int _state;
    int _kind;
    std::aligned_storage_t
        <
              sizeof( token_type )
            + sizeof( $$(ntype_type) )
        ,
        std::max
        ({
              alignof( token_type )
            + alignof( $$(ntype_type) )
        })
        >
    _storage[ 1 ];
};



/*
    Rules.
*/

$$(rule_type) $(class_name)::$$(rule_name)($$(rule_param)) { $$(rule_body) }



/*
    A piece of the parser stack.  The stack is a tree, with the leaves being
    the active stack tops.  This is how we implement generalized parsing.
*/

struct $(class_name)::piece
{
    int refcount;
    piece* prev;
    std::vector< value > values;
}



/*
    The top of an active parse stack.  They are in a doubly-linked list.
*/

struct $(class_name)::stack
{
    int state;
    piece* piece;
    stack* prev;
    stack* next;
};



/*
    Implementation of the parser.
*/

$(class_name)::$(class_name)( const user_value& u )
    :   u( u )
    ,   _anchor{ -1, nullptr, nullptr }
{
    new_stack( &_anchor, nullptr, START_STATE );
}

$(class_name)::~$(class_name)()
{
    while ( _anchor.next )
    {
        delete_stack( _anchor.next );
    }
}

void $(class_name)::parse( int token, const token_type& v )
{
    // Evaluate for each active parse stack.
    for ( stack* s = _anchor.next; s; s = s->next )
    {
        assert( s->state >= 0 );
    
        // Loop until this parse fails or we manage to shift the token.
        while ( true )
        {
            // Look up action.
            int action = ACTION[ s->state * TOKEN_COUNT + token ];
            if ( action < STATE_COUNT )
            {
                // Shift and move to the state encoded in the action.
                s->piece->values.push_back( value( s->state, v ) );
                s->state = action;
                break;
            }
            else if ( action < STATE_COUNT + RULE_COUNT )
            {
                // Reduce using the rule.
                reduce( s, action - STATE_COUNT );

                // Continue around while loop until we do something other
                // than a reduction (a reduction does not consume the token).
            }
            else if ( action < STATE_COUNT + RULE_COUNT + CONFLICT_COUNT )
            {
                // Each active stack top is a piece with no referencers, We
                // create a unique stack piece for each action.  We continue
                // from the first stack we reduced with.
                stack* loop_stack = s->prev;
                stack* z = prev;
                
                // Get list of actions in the conflict.
                const auto* conflict = CONFLICT + action - STATE_COUNT - RULE_COUNT;
                int size = conflict[ 0 ];
                assert( size >= 2 );
                
                // Only the first action may be a shift
                if ( conflict[ 1 ] < STATE_COUNT )
                {
                    // Create a new stack.
                    z = new_stack( z, s->piece, s->state );
                    
                    // Shift and move to the state encoded in the action.
                    int action = conflict[ 1 ];
                    z->piece->values.push_back( value( z->state, v ) );
                    z->state = action;
                    
                    // Ignore this stack until the next token.
                    loop_stack = z;
                }
                
                // Other actions must be reductions.
                for ( int i = 1; i < size; ++i )
                {
                    // Create a new stack.
                    z = new_stack( z, s->piece, s->state );
                
                    // Reduce using the rule.
                    int action = conflict[ i ];
                    assert( action >= STATE_COUNT && action < STATE_COUNT + RULE_COUNT );
                    reduce( z, action - STATE_COUNT );
                    
                    // If this is the first reduction, then we continue around
                    // the while loop until we do something other than a
                    // reduction (see below).  Otherwise this stack will be
                    // picked up on the next iteration of the main for loop,
                    // and the token consumed that way.
                }
                
                // Continue with the while loop using the first stack
                // resulting from a reduction.
                s = loop_stack;
            }
            else if ( action == ERROR_ACTION )
            {
                // If this is not the only stack, then destroy the stack.
                if ( s->next == &_anchor && s->prev == &_anchor )
                {
                    delete_stack( ( s = s->prev )->next );
                    break;
                }
                
                // Otherwise report the error.
                error( int token, v );
                
                // TODO: error recovery.
                return;
            }
            else if ( action == ACCEPT_ACTION )
            {
                // Everything is fine, clean up by destroying the stack.
                delete_stack( ( s = s->prev )->next );
            }
        }
    }
}

void $(class_name)::reduce( stack* s, int rule )
{
    const auto& rule_info = RULE[ rule ];

    // Find length of rule and ensure this stack has at least that many values.
    size_t length = rule_info.length;
    assert( s->piece->refcount == 0 );
    while ( s->piece->values.size() < length )
    {
        piece* prev = s->piece->prev;
        assert( prev );
        
        if ( prev->refcount == 1 )
        {
            // Move all values from this piece to the previous one.
            prev->values.reserve( prev->values.size() + s->piece->values->size() );
            std::move
            (
                s->piece->values.begin(),
                s->piece->values.end(),
                std::back_inserter( prev->values )
            );
            
            // Move stack to from previous piece. to this piece.
            std::swap( prev->values, s->piece->values );
            
            // Unlink and delete previous piece.
            s->piece->prev = prev->prev;
            delete prev;
        }
        else
        {
            // Copy values into this piece.
            size_t rq_count = length - s->piece->values.size();
            size_t cp_count = std::min( prev->values.size(), rq_count );
            size_t index = prev->values.size() - cp_count;
            s->piece->values.insert
            (
                s->piece->values.begin(),
                prev->values.begin() + index,
                prev->values.end()
            );
            
            // Split previous piece.
            if ( index > 0 )
            {
                /*
                          <- s->piece
                    split
                          <- prev <- ...
                */

                // Create split piece and link it in.
                piece* split = new piece{ 2, prev->prev };
                prev->prev = split;
                s->piece->prev = split;
                
                // Move prev's stack to the split piece.
                std::swap( split->values, prev->values );
                
                // Move values after index from split to prev.
                prev->values.reserve( cp_count );
                std::move
                (
                    split->values.begin() + index,
                    split->values.end(),
                    std::back_inserter( prev->values )
                );
            }
            else
            {
                /*
                               <- s->piece
                    prev->prev
                               <- prev <- ...
                */
                
                prev->refcount -= 1;
                assert( prev->refcount > 0 );
                s->piece->prev = prev->prev;
                prev->prev->refcount += 1;
            }
        }
    }
    
    // Get pointer to values used to reduce.
    std::vector< value >& values = s->piece->values;
    assert( values.size() >= length );
    size_t index = values.size() - length;
    value* p = values.data() + index;

    // Perform rule.
    switch ( rule )
    {
    case $$(rule_index): $$(rule_assign)$$(rule_name)($$(rule_args)) ); break;
    }
    
    // Remove excess elements.
    values.erase( values.begin() + index + 1, values.end() );
    
    // Find state we've returned to after reduction, and goto next one.
    int state = p[ index ].state();
    int gotos = GOTO[ state * NTERM_COUNT + rule_info.nterm ];
    assert( gotos < STATE_COUNT );
    s->state = gotos;
}

void $(class_name)::error( int token, const token_type& v )
{
    $(error_report)
}

$(class_name)::stack* $(class_name)::new_stack( stack* list, piece* prev, int state )
{
    // Create new piece to be the head of the stack.
    piece* p = new piece { 1, prev };
    stack* s = new stack { state, p, list, list->next };
    p->prev->refcount += 1;
    s->prev->next = s;
    s->next->prev = s;
    return s;
}

void $(class_name)::delete_stack( stack* s )
{
    // Delete stack pieces.
    while ( s->piece && s->piece->refcount == 0 )
    {
        piece* prev = s->piece->prev;
        delete s->piece;
        prev->refcount -= 1;
        s->piece = prev;
    }
    
    // Unlink and then delete stack object itself.
    s->prev->next = s->next;
    s->next->prev = s->prev;
    delete s;
}
