/*
 * Surge XT - a free and open source hybrid synthesizer,
 * built by Surge Synth Team
 *
 * Learn more at https://surge-synthesizer.github.io/
 *
 * Copyright 2018-2024, various authors, as described in the GitHub
 * transaction log.
 *
 * Surge XT is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * Surge was a commercial product from 2004-2018, copyright and ownership
 * held by Claes Johanson at Vember Audio during that period.
 * Claes made Surge open source in September 2018.
 *
 * All source for Surge XT is available at
 * https://github.com/surge-synthesizer/surge
 */

#include "PatchDB.h"

#include <tao/pegtl.hpp>
#include <tao/pegtl/analyze.hpp>
#include <tao/pegtl/contrib/raw_string.hpp>
#include <tao/pegtl/contrib/parse_tree.hpp>
#include <tao/pegtl/contrib/parse_tree_to_dot.hpp>
#include <tao/pegtl/contrib/tracer.hpp>

namespace Surge
{
namespace PatchStorage
{

namespace pegtl = tao::TAO_PEGTL_NAMESPACE;

namespace grammar
{
// CLang Format wants all these empty structs to be braced on newlines so turn it off for now
// clang-format off
struct expression;
struct value;

struct and_op : TAO_PEGTL_STRING("AND"){};
struct or_op : TAO_PEGTL_STRING("OR"){};
struct bin_op : pegtl::sor<and_op, or_op> {};
struct binary : pegtl::seq<expression, pegtl::plus<pegtl::space>, bin_op, pegtl::plus<pegtl::space>, expression> {};

struct author : TAO_PEGTL_STRING( "AUTHOR" ) {};
struct category : TAO_PEGTL_STRING( "CATEGORY" ) {};
struct auth : TAO_PEGTL_STRING( "AUTH" ) {};
struct cat : TAO_PEGTL_STRING( "CAT" ) {};
struct subsearch_keyword : pegtl::sor<author, category, auth, cat> {};

struct subsearch : pegtl::seq< subsearch_keyword, pegtl::one< '=' >, value >{};

struct keywords : pegtl::sor<bin_op, subsearch_keyword> {};

template< char C > struct string_without : pegtl::star< pegtl::not_one< C, 10, 13 > > {};
struct plain_value : pegtl::minus<pegtl::star< pegtl::not_one< ' ', '(', ')', 10, 13 > >, keywords> {};
struct quoted_value : string_without<'"'> {};
struct quoted_value_in_quotes : pegtl::if_must< pegtl::one< '"' >, quoted_value, pegtl::one< '"' > > {};
struct value : pegtl::sor<subsearch, quoted_value_in_quotes, plain_value> {};

struct value_list : pegtl::list<value, pegtl::plus<pegtl::space>> {};
struct bracketed_expression : pegtl::if_must< pegtl::one< '('>, pegtl::opt<pegtl::space>, expression, pegtl::opt<pegtl::space>, pegtl::one< ')'> > {};
struct oparg : pegtl::sor<bracketed_expression, value> {};

struct combo_op : pegtl::seq<oparg, pegtl::plus<pegtl::space>, bin_op, pegtl::plus<pegtl::space>, oparg> {};
struct expression : pegtl::sor<combo_op, bracketed_expression, value_list, value> {};
struct grammar : pegtl::seq<expression, pegtl::eof> {};

template< typename Rule >
    using selector = pegtl::parse_tree::selector<
        Rule,
        tao::pegtl::parse_tree::fold_one::on<
            >,
        tao::pegtl::parse_tree::store_content::on<
            quoted_value, plain_value, value_list, bracketed_expression, combo_op, and_op, or_op,
            subsearch, subsearch_keyword
            > >;

// clang-format on
} // namespace grammar

template <typename N> void printTree(std::ostream &os, const N &n, const std::string &pfx = "")
{
    os << pfx;
    if (!n.is_root())
        os << n.name() << " ";
    else
        os << "ROOT";
    if (n.has_content())
        os << "[" << n.content() << "] ";

    os << std::endl;
    for (auto &c : n.children)
        printTree(os, *c, pfx + "..");
}

template <typename N> std::unique_ptr<PatchDBQueryParser::Token> treeToToken(const N &n)
{
    auto t = std::make_unique<PatchDBQueryParser::Token>();

    if (n.id == std::type_index(typeid(grammar::value_list)))
    {
        if (n.children.size() == 1)
        {
            return treeToToken(*n.children[0]);
        }
        else
        {
            t->type = PatchDBQueryParser::AND;
            for (const auto &c : n.children)
            {
                auto k = treeToToken(*c);
                if (k->type != PatchDBQueryParser::TokenType::INVALID)
                    t->children.push_back(std::move(k));
            }
        }
    }
    else if (n.id == std::type_index(typeid(grammar::quoted_value)) ||
             n.id == std::type_index(typeid(grammar::plain_value)))
    {
        t->type = PatchDBQueryParser::LITERAL;
        t->content = n.content();
    }
    else if (n.id == std::type_index(typeid(grammar::bracketed_expression)))
    {
        return treeToToken(*(n.children[0]));
    }
    else if (n.id == std::type_index(typeid(grammar::combo_op)))
    {
        // we get a value binop value trio always
        auto t1 = treeToToken(*(n.children[0]));
        auto t2 = treeToToken(*(n.children[2]));
        auto opid = n.children[1]->id;
        if (opid == std::type_index(typeid(grammar::and_op)))
        {
            t->type = PatchDBQueryParser::AND;
        }
        else
        {
            t->type = PatchDBQueryParser::OR;
        }
        t->children.push_back(std::move(t1));
        t->children.push_back(std::move(t2));
    }
    else if (n.id == std::type_index(typeid(grammar::subsearch)))
    {
        t->type = PatchDBQueryParser::KEYWORD_EQUALS;
        t->content = n.children[0]->content();
        t->children.push_back(treeToToken(*n.children[1]));
    }
    else
    {
        std::cout << "UNHANDLED CASE " << n.name() << std::endl;
    }
    return t;
}

std::unique_ptr<PatchDBQueryParser::Token> PatchDBQueryParser::parseQuery(const std::string &q)
{
    // pegtl::string_input<> tin(q, "Provided Expression");
    //  pegtl::parse<grammar::combo_op, pegtl::nothing, pegtl::tracer>(tin);

    pegtl::string_input<> in(q, "Provided Expression");

    try
    {
        auto root = pegtl::parse_tree::parse<grammar::expression, grammar::selector>(in);
        if (root)
        {
            // printTree(std::cout, *root);
            if (root->children.size() == 1)
            {
                auto t = treeToToken(*(root->children[0]));
                // printParseTree(std::cout, t);
                return t;
            }

            return std::make_unique<Token>();
        }
        else
        {
            return std::make_unique<Token>();
        }
    }
    catch (const pegtl::parse_error &e)
    {
        std::cout << e.what() << std::endl;
        return std::make_unique<Token>();
    }
}

void PatchDBQueryParser::printParseTree(std::ostream &os, const std::unique_ptr<Token> &t,
                                        const std::string pfx)
{
    os << pfx;
    switch (t->type)
    {
    case INVALID:
        os << " <<ERROR>>\n";
        break;
    case AND:
        os << "AND[\n";
        for (auto &c : t->children)
            printParseTree(os, c, pfx + "..");
        os << pfx << "]\n";
        break;
    case OR:
        os << "OR[\n";
        for (auto &c : t->children)
            printParseTree(os, c, pfx + "..");
        os << pfx << "]\n";
        break;
    case LITERAL:
        os << "LITERAL [" << t->content << "]\n";
        break;
    case KEYWORD_EQUALS:
        os << "KEYWORD (" << t->content << ") [\n";
        printParseTree(os, t->children[0], pfx + "..");
        os << pfx << "]\n";
        break;
    }
}

} // namespace PatchStorage
} // namespace Surge