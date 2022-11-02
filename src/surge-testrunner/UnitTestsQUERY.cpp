#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>

#include "PatchDB.h"

#include "catch2/catch2.hpp"

TEST_CASE("Simple Query Parse", "[query]")
{
    SECTION("Single Literal")
    {
        auto t = Surge::PatchStorage::PatchDBQueryParser::parseQuery("init");
        REQUIRE(t->type == Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->content == "init");
        REQUIRE(t->children.empty());
    }
    SECTION("Two Literals are an And")
    {
        auto t = Surge::PatchStorage::PatchDBQueryParser::parseQuery("init sine");
        REQUIRE(t->type == Surge::PatchStorage::PatchDBQueryParser::AND);
        REQUIRE(t->children.size() == 2);
        REQUIRE(t->children[0]->type == Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->children[1]->type == Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->children[0]->content == "init");
        REQUIRE(t->children[1]->content == "sine");
    }
    SECTION("Multi Spaces between Literals are an And")
    {
        auto t = Surge::PatchStorage::PatchDBQueryParser::parseQuery("init    sine");
        REQUIRE(t->type == Surge::PatchStorage::PatchDBQueryParser::AND);
        REQUIRE(t->children.size() == 2);
        REQUIRE(t->children[0]->type == Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->children[1]->type == Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->children[0]->content == "init");
        REQUIRE(t->children[1]->content == "sine");
    }
    SECTION("OR explicit")
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

    SECTION("Parens")
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

    SECTION("Parens2")
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

    SECTION("Parens Ctd")
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
    SECTION("Simplest of all")
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