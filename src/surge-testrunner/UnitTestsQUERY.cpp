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
        auto q = Surge::PatchStorage::PatchDBQueryParser();
        auto t = q.parseQuery("init");
        REQUIRE(t->type == Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->content == "init");
        REQUIRE(t->children.empty());
    }
    SECTION("Two Literals are an And")
    {
        auto q = Surge::PatchStorage::PatchDBQueryParser();
        auto t = q.parseQuery("init sine");
        REQUIRE(t->type == Surge::PatchStorage::PatchDBQueryParser::AND);
        REQUIRE(t->children.size() == 2);
        REQUIRE(t->children[0]->type == Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->children[1]->type == Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->children[0]->content == "init");
        REQUIRE(t->children[1]->content == "sine");
    }
    SECTION("Multi Spaces between Literals are an And")
    {
        auto q = Surge::PatchStorage::PatchDBQueryParser();
        auto t = q.parseQuery("init    sine");
        REQUIRE(t->type == Surge::PatchStorage::PatchDBQueryParser::AND);
        REQUIRE(t->children.size() == 2);
        REQUIRE(t->children[0]->type == Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->children[1]->type == Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->children[0]->content == "init");
        REQUIRE(t->children[1]->content == "sine");
    }
    SECTION("OR explicit")
    {
        auto q = Surge::PatchStorage::PatchDBQueryParser();
        auto t = q.parseQuery("init OR sine");
        REQUIRE(t->type == Surge::PatchStorage::PatchDBQueryParser::OR);
        REQUIRE(t->children.size() == 2);
        REQUIRE(t->children[0]->type == Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->children[1]->type == Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->children[0]->content == "init");
        REQUIRE(t->children[1]->content == "sine");
    }

    SECTION("Quoted Items")
    {
        auto q = Surge::PatchStorage::PatchDBQueryParser();
        auto t = q.parseQuery("\"init sine\"");
        REQUIRE(t->type == Surge::PatchStorage::PatchDBQueryParser::LITERAL);
        REQUIRE(t->content == "init sine");
        REQUIRE(t->children.empty());
    }

    SECTION("Quoted Items Mixed")
    {
        auto q = Surge::PatchStorage::PatchDBQueryParser();
        auto t = q.parseQuery("\"init sine\" OR dx7");
        REQUIRE(t->type == Surge::PatchStorage::PatchDBQueryParser::OR);
        REQUIRE(t->children.size() == 2);
        REQUIRE(t->children[0]->content == "init sine");
        REQUIRE(t->children[1]->content == "dx7");
    }

    SECTION("Parens")
    {
        auto q = Surge::PatchStorage::PatchDBQueryParser();
        auto t = q.parseQuery("(init sine) OR dx7");
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
        auto q = Surge::PatchStorage::PatchDBQueryParser();
        auto t = q.parseQuery("dx7 OR (init sine)");
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
        auto q = Surge::PatchStorage::PatchDBQueryParser();
        auto t = q.parseQuery("(esq1 OR (init sine)) AND dx7");
        REQUIRE(t->type == Surge::PatchStorage::PatchDBQueryParser::AND);
        REQUIRE(t->children[0]->type == Surge::PatchStorage::PatchDBQueryParser::OR);
        REQUIRE(t->children[1]->type == Surge::PatchStorage::PatchDBQueryParser::LITERAL);
    }
}