#include <gtest/gtest.h>
#ifndef TEST
#define TEST
#endif
#include "IrcParser.cpp"

TEST(IrcParser, BasicMessages)
{
    IrcParser parser;
    IrcMessage msg;
    bool res = false;

    parser.push("NICK Test\n\r");
    res = parser.run(msg);
    EXPECT_TRUE(res);
    if (res) {
        EXPECT_EQ(msg.command, "NICK");
        EXPECT_EQ(msg.params.size(), 1);
        if (msg.params.size() > 0) {
            EXPECT_EQ(msg.params[0], "Test");
        }
    }
    parser.push("USER Foo Bar Baz\r\n");
    res = parser.run(msg);
    EXPECT_TRUE(res);
    if (res) {
        EXPECT_EQ(msg.command, "USER");
        EXPECT_EQ(msg.params.size(), 3);
        if (msg.params.size() == 3) {
            EXPECT_EQ(msg.params[0], "Foo");
            EXPECT_EQ(msg.params[1], "Bar");
            EXPECT_EQ(msg.params[2], "Baz");
        }
    }
    parser.push("FOO");
    parser.push("BAR\r\n");
    res = parser.run(msg);
    EXPECT_TRUE(res);
    if (res) {
        EXPECT_EQ(msg.command, "FOOBAR");
    }
    parser.push(":nick!user@host cmd 1 2 :3\r\n");
    res = parser.run(msg);
    EXPECT_TRUE(res);
    if (res) {
        EXPECT_EQ(msg.nickname, "nick");
        EXPECT_EQ(msg.user, "user");
        EXPECT_EQ(msg.host, "host");
        EXPECT_EQ(msg.command, "cmd");
        EXPECT_EQ(msg.params.size(), 3);
        EXPECT_TRUE(msg.trailing);
        EXPECT_EQ(msg.params[0], "1");
        EXPECT_EQ(msg.params[1], "2");
        EXPECT_EQ(msg.params[2], "3");
    }
}
