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
#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "PatchDB.h"

#include "catch2/catch_amalgamated.hpp"

TEST_CASE("Simple Query Parse", "[query]")
{
    SECTION("Single Literal")
    {
        auto t = Surge::PatchStorage::PatchDBQueryParser::parseQuery("init");
        REQUIRE(t->type == Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->content == "init");
        REQUIRE(t->children.empty());
    }
    SECTION("Two Literals Are an AND")
    {
        auto t = Surge::PatchStorage::PatchDBQueryParser::parseQuery("init sine");
        REQUIRE(t->type == Surge::PatchStorage::PatchDBQueryParser::AND);
        REQUIRE(t->children.size() == 2);
        REQUIRE(t->children[0]->type == Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->children[1]->type == Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->children[0]->content == "init");
        REQUIRE(t->children[1]->content == "sine");
    }
    SECTION("Multi Spaces Between Literals Are an AND")
    {
        auto t = Surge::PatchStorage::PatchDBQueryParser::parseQuery("init    sine");
        REQUIRE(t->type == Surge::PatchStorage::PatchDBQueryParser::AND);
        REQUIRE(t->children.size() == 2);
        REQUIRE(t->children[0]->type == Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->children[1]->type == Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->children[0]->content == "init");
        REQUIRE(t->children[1]->content == "sine");
    }
    SECTION("OR Explicit")
    {
        auto t = Surge::PatchStorage::PatchDBQueryParser::parseQuery("init OR sine");
        REQUIRE(t->type == Surge::PatchStorage::PatchDBQueryParser::OR);
        REQUIRE(t->children.size() == 2);
        REQUIRE(t->children[0]->type == Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->children[1]->type == Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->children[0]->content == "init");
        REQUIRE(t->children[1]->content == "sine");
    }

    SECTION("Quoted Items")
    {
        auto t = Surge::PatchStorage::PatchDBQueryParser::parseQuery("\"init sine\"");
        REQUIRE(t->type == Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->content == "init sine");
        REQUIRE(t->children.empty());
    }

    SECTION("Quoted Items Mixed")
    {
        auto t = Surge::PatchStorage::PatchDBQueryParser::parseQuery("\"init sine\" OR dx7");
        REQUIRE(t->type == Surge::PatchStorage::PatchDBQueryParser::OR);
        REQUIRE(t->children.size() == 2);
        REQUIRE(t->children[0]->content == "init sine");
        REQUIRE(t->children[1]->content == "dx7");
    }

    SECTION("Parentheses Pt 1")
    {
        auto t = Surge::PatchStorage::PatchDBQueryParser::parseQuery("(init sine) OR dx7");
        REQUIRE(t->type == Surge::PatchStorage::PatchDBQueryParser::OR);
        REQUIRE(t->children[0]->type == Surge::PatchStorage::PatchDBQueryParser::AND);
        REQUIRE(t->children[0]->children[0]->type ==
                Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->children[0]->children[0]->content == "init");
        REQUIRE(t->children[0]->children[1]->type ==
                Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->children[0]->children[1]->content == "sine");
        REQUIRE(t->children[1]->type == Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->children[1]->content == "dx7");
    }

    SECTION("Parentheses Pt 2")
    {
        auto t = Surge::PatchStorage::PatchDBQueryParser::parseQuery("dx7 OR (init sine)");
        REQUIRE(t->children[0]->type == Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->children[0]->content == "dx7");

        REQUIRE(t->type == Surge::PatchStorage::PatchDBQueryParser::OR);
        REQUIRE(t->children[1]->type == Surge::PatchStorage::PatchDBQueryParser::AND);
        REQUIRE(t->children[1]->children[0]->type ==
                Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->children[1]->children[0]->content == "init");
        REQUIRE(t->children[1]->children[1]->type ==
                Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->children[1]->children[1]->content == "sine");
    }

    SECTION("Parentheses Pt 3")
    {
        auto t =
            Surge::PatchStorage::PatchDBQueryParser::parseQuery("(esq1 OR (init sine)) AND dx7");
        REQUIRE(t->type == Surge::PatchStorage::PatchDBQueryParser::AND);
        REQUIRE(t->children[0]->type == Surge::PatchStorage::PatchDBQueryParser::OR);
        REQUIRE(t->children[1]->type == Surge::PatchStorage::PatchDBQueryParser::LITERAL);
    }

    SECTION("Simple Keywords")
    {
        auto t = Surge::PatchStorage::PatchDBQueryParser::parseQuery("init AUTHOR=bacon");
        REQUIRE(t->type == Surge::PatchStorage::PatchDBQueryParser::AND);
        REQUIRE(t->children[0]->type == Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->children[1]->type == Surge::PatchStorage::PatchDBQueryParser::KEYWORD_EQUALS);
        REQUIRE(t->children[1]->content == "AUTHOR");
        REQUIRE(t->children[1]->children[0]->type ==
                Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->children[1]->children[0]->content == "bacon");
    }

    SECTION("Compound Keywords")
    {
        auto t = Surge::PatchStorage::PatchDBQueryParser::parseQuery(
            "(pad OR atmosphere) AND (AUTHOR=bacon OR AUTHOR=Jacky)");
        REQUIRE(t->children[1]->children[1]->type ==
                Surge::PatchStorage::PatchDBQueryParser::KEYWORD_EQUALS);
        REQUIRE(t->children[1]->children[0]->type ==
                Surge::PatchStorage::PatchDBQueryParser::KEYWORD_EQUALS);
    }

    // ADDRESS ["init sine" OR dx pad] drops the pad
}

TEST_CASE("SQL Generation", "[query]")
{
    SECTION("Simplest of All")
    {
        auto t = Surge::PatchStorage::PatchDBQueryParser::parseQuery("init");
        auto s = Surge::PatchStorage::PatchDB::sqlWhereClauseFor(t);
        REQUIRE(s == "( p.search_over LIKE '%init%' )");
    }

    SECTION("Duple")
    {
        auto t = Surge::PatchStorage::PatchDBQueryParser::parseQuery("init sine");
        auto s = Surge::PatchStorage::PatchDB::sqlWhereClauseFor(t);
        REQUIRE(s == "( ( p.search_over LIKE '%init%' ) AND ( p.search_over LIKE '%sine%' ) )");
    }

    SECTION("Single Quote")
    {
        auto t = Surge::PatchStorage::PatchDBQueryParser::parseQuery("init 'sine");
        auto s = Surge::PatchStorage::PatchDB::sqlWhereClauseFor(t);
        REQUIRE(s == "( ( p.search_over LIKE '%init%' ) AND ( p.search_over LIKE '%''sine%' ) )");
    }

    SECTION("Lots of Single Quotes Quote")
    {
        auto t = Surge::PatchStorage::PatchDBQueryParser::parseQuery("in'it' ''sine");
        auto s = Surge::PatchStorage::PatchDB::sqlWhereClauseFor(t);
        REQUIRE(s ==
                "( ( p.search_over LIKE '%in''it''%' ) AND ( p.search_over LIKE '%''''sine%' ) )");
    }
}