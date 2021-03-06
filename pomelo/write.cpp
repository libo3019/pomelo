//
//  write.cpp
//  pomelo
//
//  Created by Edmund Kapusniak on 02/02/2018.
//  Copyright © 2018 Edmund Kapusniak.
//
//  Licensed under the MIT License. See LICENSE file in the project root for
//  full license information. 
//


#include "write.h"
#include <assert.h>
#include <string_view>


namespace
{
#include "template.inc"
}



write::write
    (
        automata_ptr automata,
        action_table_ptr action_table,
        goto_table_ptr goto_table,
        const std::string& source,
        const std::string& output_h
    )
    :   _automata( automata )
    ,   _action_table( action_table )
    ,   _goto_table( goto_table )
    ,   _source( source )
    ,   _output_h( output_h )
{
}

write::~write()
{
}


std::string write::trim( const std::string& s )
{
    size_t lower = s.find_first_not_of( " \t" );
    size_t upper = s.find_last_not_of( " \t" );
    if ( lower != std::string::npos && upper != std::string::npos )
    {
        return s.substr( lower, upper + 1 - lower );
    }
    else
    {
        return "";
    }
}

bool write::starts_with( const std::string& s, const std::string& z )
{
    return s.compare( 0, z.size(), z ) == 0;
}


void write::prepare()
{
    // Munge header name.
    size_t pos = _output_h.find_last_of( "/\\" );
    if ( pos != std::string::npos )
    {
        _output_h = _output_h.substr( pos + 1 );
    }

    // Build in-order list of terminals.
    for ( const auto& tsym : _automata->syntax->terminals )
    {
        _tokens.push_back( tsym.second.get() );
    }
    std::sort
    (
        _tokens.begin(),
        _tokens.end(),
        []( terminal* a, terminal* b ) { return a->value < b->value; }
    );

    // Build in-order list of nonterminals.
    for ( const auto& nsym : _automata->syntax->nonterminals )
    {
        _nterms.push_back( nsym.second.get() );
    }
    std::sort
    (
        _nterms.begin(),
        _nterms.end(),
        []( nonterminal* a, nonterminal* b ) { return a->value < b->value; }
    );
    
    
    // Add the token type.
    std::unordered_map< std::string, ntype* > lookup;
    std::unique_ptr< ntype > n = std::make_unique< ntype >();
    std::string type;
    if ( _automata->syntax->token_type.specified )
        type = trim( _automata->syntax->token_type.text );
    else
        type = "std::nullptr_t";
    
    ntype* resolved = n.get();
    n->ntype = type;
    n->value = (int)_ntypes.size();
    lookup.emplace( n->ntype, resolved );
    _ntypes.push_back( std::move( n ) );
    
    // Work out nonterminal types for each nonterminal.
    for ( nonterminal* nterm : _nterms )
    {
        std::string type = trim( nterm->type );
        if ( type.empty() )
        {
            type = "std::nullptr_t";
        }
    
        ntype* resolved;
        auto i = lookup.find( type );
        if ( i != lookup.end() )
        {
            resolved = i->second;
        }
        else
        {
            std::unique_ptr< ntype > n = std::make_unique< ntype >();
            n->ntype = type;
            n->value = (int)_ntypes.size();
            resolved = n.get();
            lookup.emplace( type, resolved );
            _ntypes.push_back( std::move( n ) );
        }
        
        _nterm_lookup.emplace( nterm, resolved );
    }
}





/*
    Here are the interpolated values we fill in in the template:
 
        $(header)
        $(include)
        $(include_guard)
        $(class_name)
        $(user_value)
        $(token_type)
        $(start_state)
        $(error_action)
        $(accept_action)
        $(token_count)
        $(nterm_count)
        $(state_count)
        $(rule_count)
        $(conflict_count)
        $(error_report)
 
    Tables:
 
        $(action_table)
        $(conflict_table)
        $(goto_table)
        $(rule_table)
 
    Per-token:
 
        $$(token_name)
        $$(token_value)
 
    Per-non-terminal:
 
        $$(nterm_name)
        $$(nterm_value)

    Per-mergable non-terminal:

        $$(merge_type)
        $$(merge_name)
        $$(merge_index)
        $$(merge_body)
 
    Per-non-terminal-type:
 
        $$(ntype_type)
        $$(ntype_destroy)
        $$(ntype_value)
 
    Per-rule:
 
        $$(rule_type)
        $$(rule_name)
        $$(rule_param)
        $$(rule_body)
        $$(rule_assign)
        $$(rule_args)
 
*/


bool write::write_header( FILE* f )
{
    return write_template( f, (char*)template_h, template_h_len, true );
}

bool write::write_source( FILE* f )
{
    return write_template( f, (char*)template_cpp, template_cpp_len, false );
}


bool write::write_template( FILE* f, char* source, size_t length, bool header )
{
    // Process file line-by-line.
    size_t i = 0;
    while ( i < length )
    {
        // Get to end of line.
        size_t iend = i;
        while ( iend < length )
        {
            if ( source[ iend ] == '\n' )
            {
                iend += 1;
                break;
            }
            iend += 1;
        }
        
        // Get line.
        std::string line( source + i, iend - i );
        std::string output;
        
        if ( line[ 0 ] == '?' || line[ 0 ] == '!' )
        {
            syntax_ptr syntax = _automata->syntax;

            bool skip = false;

            if ( starts_with( line, "?(user_value)" ) )
            {
                skip = skip || ! syntax->user_value.specified;
                line = line.substr( line.find( ')' ) + 1 );
            }
            if ( starts_with( line, "!(user_value)" ) )
            {
                skip = skip || syntax->user_value.specified;
                line = line.substr( line.find( ')' ) + 1 );
            }
            if ( starts_with( line, "?(token_type)" ) )
            {
                skip = skip || ! syntax->token_type.specified;
                line = line.substr( line.find( ')' ) + 1 );
            }
            if ( starts_with( line, "!(token_type)" ) )
            {
                skip = skip || syntax->token_type.specified;
                line = line.substr( line.find( ')' ) + 1 );
            }

            if ( skip )
            {
                i = iend;
                continue;
            }
        }

        // Check for interpolants.
        size_t per = line.find( "$$(" );
        if ( per != std::string::npos )
        {
            if ( line.compare( per, 9, "$$(token_" ) == 0 )
            {
                for ( terminal* token : _tokens )
                {
                    if ( token->is_special )
                    {
                        continue;
                    }
                    output += replace( replace( line, token ) );
                }
            }
            else if ( line.compare( per, 9, "$$(merge_" ) == 0 )
            {
                for ( nonterminal* nterm : _nterms )
                {
                    if ( ! nterm->gspecified )
                    {
                        continue;
                    }
                    output += replace( replace( line, nterm ) );
                }
            }
            else if ( line.compare( per, 9, "$$(nterm_" ) == 0 )
            {
                for ( nonterminal* nterm : _nterms )
                {
                    if ( nterm->is_special )
                    {
                        continue;
                    }
                    output += replace( replace( line, nterm ) );
                }
            }
            else if ( line.compare( per, 9, "$$(ntype_" ) == 0 )
            {
                for ( const auto& ntype : _ntypes )
                {
                    output += replace( replace( line, ntype.get() ) );
                }
            }
            else if ( line.compare( per, 8, "$$(rule_" ) == 0 )
            {
                for ( const auto& rule : _automata->syntax->rules )
                {
                    output += replace( replace( line, rule.get(), header ) );
                }
            }
            else
            {
                assert( ! "invalid template" );
            }
        }
        else if ( line.find( "$(" ) != std::string::npos )
        {
            output = replace( line );
        }
        else
        {
            output = line;
        }
        
        if ( fwrite( output.data(), 1, output.size(), f ) < output.size() )
        {
            fprintf( stderr, "error writing output file" );
            return false;
        }
        
        i = iend;
    }
    
    return true;
}


struct replacer
{
    std::string& line;
    const char* prefix;
    size_t i;
    size_t lower;
    size_t length;
    
    replacer( std::string& line, const char* prefix )
        :   line( line )
        ,   prefix( prefix )
        ,   i( 0 )
        ,   lower( 0 )
        ,   length( 0 )
    {
    }
    
    bool next( std::string_view& valname )
    {
        i = lower;
        if ( ( lower = line.find( prefix, i ) ) != std::string::npos )
        {
            size_t upper = line.find( ')', lower );
            assert( upper != std::string::npos );
            length = upper + 1 - lower;
            valname = std::string_view( line ).substr( lower, length );
            return true;
        }
        else
        {
            return false;
        }
    }
    
    void replace( const std::string& s )
    {
        line.replace( lower, length, s );
    }
};


std::string write::replace( std::string line )
{
    /*
        $(header)
        $(include_header)
        $(include_source)
        $(include_guard)
        $(class_name)
        $(token_prefix)
        $(nterm_prefix)
        $(user_value)
        $(user_split)
        $(token_type)
        $(start_state)
        $(error_action)
        $(accept_action)
        $(token_count)
        $(nterm_count)
        $(state_count)
        $(rule_count)
        $(conflict_count)
        $(error_report)

        $(action_table)
        $(action_displacement)
        $(action_value_table)
        $(action_row_table)
        $(goto_table)
        $(goto_displacement)
        $(goto_value_table)
        $(goto_row_table)
        $(conflict_table)
        $(rule_table)
    */
    
    syntax_ptr syntax = _automata->syntax;
    replacer r( line, "$(" );
    std::string_view valname;
    while ( r.next( valname ) )
    {
        if ( valname == "$(header)" )
        {
            r.replace( _output_h );
        }
        else if ( valname == "$(include_header)" )
        {
            r.replace( trim( syntax->include_header.text ) );
        }
        else if ( valname == "$(include_source)" )
        {
            r.replace( trim( syntax->include_source.text ) );
        }
        else if ( valname == "$(include_guard)" )
        {
            std::string guard = _output_h;
            std::transform( guard.begin(), guard.end(), guard.begin(), ::toupper );
            size_t period = guard.find( '.' );
            if ( period != std::string::npos )
            {
                guard[ period ] = '_';
            }
            r.replace( guard );
        }
        else if ( valname == "$(class_name)" )
        {
            std::string name;
            if ( syntax->class_name.specified )
                name = trim( syntax->class_name.text );
            else
                name = "parser";
            r.replace( name );
        }
        else if ( valname == "$(token_prefix)" )
        {
            std::string prefix;
            if ( syntax->token_prefix.specified )
                prefix = trim( syntax->token_prefix.text );
            else
                prefix = "TOKEN_";
            r.replace( prefix );
        }
        else if ( valname == "$(nterm_prefix)" )
        {
            std::string prefix;
            if ( syntax->nterm_prefix.specified )
                prefix = trim( syntax->nterm_prefix.text );
            else
                prefix = "NTERM_";
            r.replace( prefix );
        }
        else if ( valname == "$(user_value)" )
        {
            r.replace( trim( syntax->user_value.text ) );
        }
        else if ( valname == "$(user_split)" )
        {
            std::string split;
            if ( syntax->user_split.specified )
                split = trim( syntax->user_split.text );
            else
                split = "return u;";
            r.replace( split );
        }
        else if ( valname == "$(token_type)" )
        {
            r.replace( trim( syntax->token_type.text ) );
        }
        else if ( valname == "$(start_state)" )
        {
            r.replace( std::to_string( _automata->start->index ) );
        }
        else if ( valname == "$(error_action)" )
        {
            r.replace( std::to_string( _action_table->error_action ) );
        }
        else if ( valname == "$(accept_action)" )
        {
            r.replace( std::to_string( _action_table->accept_action ) );
        }
        else if ( valname == "$(token_count)" )
        {
            r.replace( std::to_string( _action_table->token_count ) );
        }
        else if ( valname == "$(nterm_count)" )
        {
            r.replace( std::to_string( _goto_table->nterm_count ) );
        }
        else if ( valname == "$(state_count)" )
        {
            r.replace( std::to_string( _action_table->state_count ) );
        }
        else if ( valname == "$(rule_count)" )
        {
            r.replace( std::to_string( _action_table->rule_count ) );
        }
        else if ( valname == "$(conflict_count)" )
        {
            r.replace( std::to_string( _action_table->conflict_count ) );
        }
        else if ( valname == "$(error_report)" )
        {
            r.replace( trim( syntax->error_report.text ) );
        }
        else if ( valname == "$(action_table)" )
        {
            r.replace( write_table( _action_table->actions ) );
        }
        else if ( valname == "$(action_displacement)" )
        {
            r.replace( write_table( _action_table->compressed->displace ) );
        }
        else if ( valname == "$(action_value_table)" )
        {
            r.replace( write_table( _action_table->compressed->compress ) );
        }
        else if ( valname == "$(action_row_table)" )
        {
            r.replace( write_table( _action_table->compressed->comprows ) );
        }
        else if ( valname == "$(goto_table)" )
        {
            r.replace( write_table( _goto_table->gotos ) );
        }
        else if ( valname == "$(goto_displacement)" )
        {
            r.replace( write_table( _goto_table->compressed->displace ) );
        }
        else if ( valname == "$(goto_value_table)" )
        {
            r.replace( write_table( _goto_table->compressed->compress ) );
        }
        else if ( valname == "$(goto_row_table)" )
        {
            r.replace( write_table( _goto_table->compressed->comprows ) );
        }
        else if ( valname == "$(conflict_table)" )
        {
            r.replace( write_table( _action_table->conflicts ) );
        }
        else if ( valname == "$(rule_table)" )
        {
            r.replace( write_rule_table() );
        }
        else
        {
            fprintf( stdout, "%.*s", (int)valname.size(), valname.data() );
            fflush( stdout );
            assert( ! "invalid template" );
        }
    }
    
    return line;
}

std::string write::replace( std::string line, terminal* token )
{
    /*
        $$(token_name)
        $$(token_value)
    */

    syntax_ptr syntax = _automata->syntax;
    replacer r( line, "$$(" );
    std::string_view valname;
    while ( r.next( valname ) )
    {
        if ( valname == "$$(token_name)" )
        {
            r.replace( syntax->source->text( token->name ) );
        }
        else if ( valname == "$$(token_value)" )
        {
            r.replace( std::to_string( token->value ) );
        }
        else
        {
            assert( ! "invalid template" );
        }
    }

    return line;
}

std::string write::replace( std::string line, nonterminal* nterm )
{
    /*
        $$(nterm_name)
        $$(nterm_value)
    */
    
    syntax_ptr syntax = _automata->syntax;
    replacer r( line, "$$(" );
    std::string_view valname;
    while ( r.next( valname ) )
    {
        if ( valname == "$$(nterm_name)" )
        {
            std::string name = syntax->source->text( nterm->name );
            r.replace( name );
        }
        else if ( valname == "$$(nterm_upper)" )
        {
            std::string name = syntax->source->text( nterm->name );
            std::transform( name.begin(), name.end(), name.begin(), ::toupper );
            r.replace( name );
        }
        else if ( valname == "$$(nterm_value)" )
        {
            r.replace( std::to_string( nterm->value ) );
        }
        else if ( valname == "$$(merge_index)" )
        {
            int index = nterm->value - _action_table->token_count;
            r.replace( std::to_string( index ) );
        }
        else if ( valname == "$$(merge_type)" )
        {
            r.replace( _nterm_lookup.at( nterm )->ntype );
        }
        else if ( valname == "$$(merge_name)" )
        {
            std::string name = "merge_";
            name += syntax->source->text( nterm->name );
            r.replace( name );
        }
        else if ( valname == "$$(merge_body)" )
        {
            std::string body;
/*            if ( nterm->gline >= 0 )
            {
                body += "\n#line ";
                body += std::to_string( nterm->gline );
                body += " \"";
                body += _source;
                body += "\"\n";
            }
*/            body += nterm->gmerge;
            body += "\n";
            r.replace( body );
        }
        else
        {
            assert( ! "invalid template" );
        }
    }

    return line;
}

std::string write::replace( std::string line, ntype* ntype )
{
    /*
        $$(ntype_type)
        $$(ntype_value)
    */
    
    if ( ntype->ntype == "std::nullptr_t" && line.find( "$$(ntype_destroy)" ) != std::string::npos )
    {
        // clang complains about calling ~nullptr_t().
        return "";
    }
    
    replacer r( line, "$$(" );
    std::string_view valname;
    while ( r.next( valname ) )
    {
        if ( valname == "$$(ntype_type)" )
        {
            r.replace( ntype->ntype );
        }
        else if ( valname == "$$(ntype_destroy)" )
        {
            std::string destroy = ntype->ntype;
            size_t pos = destroy.find_last_of( ':' );
            if ( pos != std::string::npos )
            {
                destroy = destroy.substr( pos + 1 );
            }
            r.replace( destroy );
        }
        else if ( valname == "$$(ntype_value)" )
        {
            r.replace( std::to_string( ntype->value ) );
        }
        else
        {
            assert( ! "invalid template" );
        }
    }

    return line;
}

std::string write::replace( std::string line, rule* rule, bool header )
{
    /*
        $$(rule_type)
        $$(rule_name)
        $$(rule_param)
        $$(rule_body)
        $$(rule_index)
        $$(rule_assign)
        $$(rule_args)
    */

    syntax_ptr syntax = _automata->syntax;
    replacer r( line, "$$(" );
    std::string_view valname;
    while ( r.next( valname ) )
    {
        if ( valname == "$$(rule_type)" )
        {
            r.replace( _nterm_lookup.at( rule->nterm )->ntype );
        }
        else if ( valname == "$$(rule_name)" )
        {
            r.replace( std::string( "rule_" ) + std::to_string( rule->index ) );
        }
        else if ( valname == "$$(rule_param)" )
        {
            std::string prm;
            if ( syntax->user_value.specified )
            {
                prm += " const user_value& u";
            }
            for ( size_t i = 0; i < rule->locount - 1; ++i )
            {
                size_t iloc = rule->lostart + i;
                const location& loc = syntax->locations.at( iloc );
                if ( ! loc.sparam )
                {
                    continue;
                }

                if ( prm.size() )
                {
                    prm += ",";
                }
                if ( loc.sym->is_terminal )
                {
                    prm += " token_type&&";
                }
                else
                {
                    prm += " ";
                    nonterminal* nterm = (nonterminal*)loc.sym;
                    prm += _nterm_lookup.at( nterm )->ntype;
                    prm += "&&";
                }
                prm += " ";
                prm += syntax->source->text( loc.sparam );
            }
            if ( prm.size() )
            {
                prm += " ";
            }

            r.replace( prm );
        }
        else if ( valname == "$$(rule_body)" )
        {
            std::string body;
            if ( rule->actspecified )
            {
/*                if ( rule->actline >= 0 )
                {
                    body += "\n#line ";
                    body += std::to_string( rule->actline );
                    body += " \"";
                    body += _source;
                    body += "\"\n";
                }
*/                body += rule->action;
                body += "\n";
            }
            else
            {
                body += "return ";
                body += _nterm_lookup.at( rule->nterm )->ntype;
                body += "();";
            }
            r.replace( body );
        }
        else if ( valname == "$$(rule_index)" )
        {
            r.replace( std::to_string( rule->index ) );
        }
        else if ( valname == "$$(rule_assign)" )
        {
            if ( rule->locount > 1 )
            {
                r.replace( "p[ 0 ] = value( p[ 0 ].state(), " );
            }
            else
            {
                r.replace( "values.emplace_back( s->state, " );
            }
        }
        else if ( valname == "$$(rule_args)" )
        {
            std::string args;
            if ( syntax->user_value.specified )
            {
                args += " s->u";
            }
            for ( size_t i = 0; i < rule->locount - 1; ++i )
            {
                size_t iloc = rule->lostart + i;
                const location& loc = syntax->locations.at( iloc );
                if ( ! loc.sparam )
                {
                    continue;
                }

                if ( args.size() )
                {
                    args += ",";
                }
                args += " p[ ";
                args += std::to_string( i );
                args += " ].move< ";
                if ( loc.sym->is_terminal )
                {
                    args += "token_type";
                }
                else
                {
                    nonterminal* nterm = (nonterminal*)loc.sym;
                    args += _nterm_lookup.at( nterm )->ntype;
                }
                args += " >()";
            }
            if ( args.size() )
            {
                args += " ";
            }
        
            r.replace( args );
        }
        else
        {
            assert( ! "invalid template" );
        }
    }

    return line;
}


std::string write::write_table( const std::vector< int >& table )
{
    std::string s;
    for ( size_t i = 0; i < table.size(); ++i )
    {
        if ( i % 20 == 0 )
        {
            if ( i > 0 )
            {
                s += "\n";
            }
            s += "   ";
        }
        s += " ";
        s += std::to_string( table.at( i ) );
        s += ",";
    }
    s += "\n";
    return s;
}


std::string write::write_rule_table()
{
    int token_count = (int)_automata->syntax->terminals.size();
    source_ptr source = _automata->syntax->source;

    std::string s;
    for ( size_t i = 0; i < _automata->syntax->rules.size(); ++i )
    {
        rule* rule = _automata->syntax->rules.at( i ).get();
        s += "    { ";
        s += std::to_string( rule->nterm->value - token_count );
        s += ", ";
        s += std::to_string( (int)rule->locount - 1 );
        s += ", ";
        s += rule->nterm->gspecified ? "1" : "0";
        s += " }, // ";

        s += source->text( rule->nterm->name );
        s += " :";
        for ( size_t i = 0; i < rule->locount - 1; ++i )
        {
            size_t iloc = rule->lostart + i;
            const location& loc = _automata->syntax->locations.at( iloc );
            s += " ";
            s += source->text( loc.sym->name );
        }
        
        s += "\n";
    }
    
    return s;
}


