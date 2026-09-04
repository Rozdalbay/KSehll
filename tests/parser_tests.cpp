#include "test_framework.h"

#include "parser/Lexer.h"
#include "parser/Parser.h"
#include "parser/Command.h"

using namespace kshell;

static bool testLexerSimpleCommand()
{
    Lexer lexer(L"echo hello");
    auto tokens = lexer.tokenize();
    TEST_ASSERT(tokens.size() >= 3, L"Expected at least 3 tokens");
    TEST_ASSERT_EQ(tokens[0].type, TokenType::Word, L"First token should be Word");
    TEST_ASSERT_EQ(tokens[0].value, std::wstring(L"echo"), L"First token should be 'echo'");
    TEST_ASSERT_EQ(tokens[1].type, TokenType::Word, L"Second token should be Word");
    TEST_ASSERT_EQ(tokens[1].value, std::wstring(L"hello"), L"Second token should be 'hello'");
    return true;
}

static bool testLexerArguments()
{
    Lexer lexer(L"program arg1 arg2 arg3");
    auto tokens = lexer.tokenize();
    TEST_ASSERT(tokens.size() >= 5, L"Expected at least 5 tokens");
    TEST_ASSERT_EQ(tokens[0].value, std::wstring(L"program"), L"program");
    TEST_ASSERT_EQ(tokens[1].value, std::wstring(L"arg1"), L"arg1");
    TEST_ASSERT_EQ(tokens[2].value, std::wstring(L"arg2"), L"arg2");
    TEST_ASSERT_EQ(tokens[3].value, std::wstring(L"arg3"), L"arg3");
    return true;
}

static bool testLexerQuotes()
{
    Lexer lexer(L"echo \"hello world\"");
    auto tokens = lexer.tokenize();
    TEST_ASSERT(tokens.size() >= 3, L"Expected 3 tokens");
    TEST_ASSERT_EQ(tokens[1].value, std::wstring(L"hello world"), L"Quoted string");
    return true;
}

static bool testLexerSingleQuotes()
{
    Lexer lexer(L"echo 'hello world'");
    auto tokens = lexer.tokenize();
    TEST_ASSERT(tokens.size() >= 3, L"Expected 3 tokens");
    TEST_ASSERT_EQ(tokens[1].value, std::wstring(L"hello world"), L"Single quoted");
    return true;
}

static bool testLexerEscaping()
{
    Lexer lexer(L"echo \"hello \\\"world\\\"\"");
    auto tokens = lexer.tokenize();
    TEST_ASSERT(tokens.size() >= 3, L"Expected 3 tokens");
    TEST_ASSERT_EQ(tokens[1].value, std::wstring(L"hello \"world\""), L"Escaped quotes");
    return true;
}

static bool testLexerPipe()
{
    Lexer lexer(L"dir | findstr cpp");
    auto tokens = lexer.tokenize();
    bool foundPipe = false;
    for (const auto& t : tokens)
    {
        if (t.type == TokenType::Pipe)
            foundPipe = true;
    }
    TEST_ASSERT(foundPipe, L"Expected pipe token");
    return true;
}

static bool testLexerRedirection()
{
    Lexer lexer(L"echo hello > file.txt");
    auto tokens = lexer.tokenize();
    bool foundGreater = false;
    for (const auto& t : tokens)
    {
        if (t.type == TokenType::Greater)
            foundGreater = true;
    }
    TEST_ASSERT(foundGreater, L"Expected > token");
    return true;
}

static bool testLexerAppend()
{
    Lexer lexer(L"echo hello >> file.txt");
    auto tokens = lexer.tokenize();
    bool found = false;
    for (const auto& t : tokens)
    {
        if (t.type == TokenType::DoubleGreater)
            found = true;
    }
    TEST_ASSERT(found, L"Expected >> token");
    return true;
}

static bool testLexerBackground()
{
    Lexer lexer(L"longcmd &");
    auto tokens = lexer.tokenize();
    bool found = false;
    for (const auto& t : tokens)
    {
        if (t.type == TokenType::Background)
            found = true;
    }
    TEST_ASSERT(found, L"Expected & token");
    return true;
}

static bool testLexerVariable()
{
    Lexer lexer(L"echo $PATH");
    auto tokens = lexer.tokenize();
    bool foundVar = false;
    for (const auto& t : tokens)
    {
        if (t.type == TokenType::Variable && t.value == L"$PATH")
            foundVar = true;
    }
    TEST_ASSERT(foundVar, L"Expected variable token");
    return true;
}

static bool testLexerPercentVariable()
{
    Lexer lexer(L"echo %PATH%");
    auto tokens = lexer.tokenize();
    bool foundVar = false;
    for (const auto& t : tokens)
    {
        if (t.type == TokenType::Word && t.value == L"%PATH%")
            foundVar = true;
    }
    TEST_ASSERT(foundVar, L"Expected %PATH% variable token");
    return true;
}

static bool testLexerErrorRedirect()
{
    Lexer lexer(L"program 2> errors.txt");
    auto tokens = lexer.tokenize();
    bool found = false;
    for (const auto& t : tokens)
    {
        if (t.type == TokenType::ErrorOutput)
            found = true;
    }
    TEST_ASSERT(found, L"Expected 2> token");
    return true;
}

static bool testLexerErrorAppend()
{
    Lexer lexer(L"program 2>> errors.txt");
    auto tokens = lexer.tokenize();
    bool found = false;
    for (const auto& t : tokens)
    {
        if (t.type == TokenType::ErrorAppend)
            found = true;
    }
    TEST_ASSERT(found, L"Expected 2>> token");
    return true;
}

static bool testParserCommand()
{
    Lexer lexer(L"echo hello world");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    std::optional<ParseError> error;
    auto pipelines = parser.parse(error);
    TEST_ASSERT(!error, L"No parse error expected");
    TEST_ASSERT(pipelines.size() == 1, L"Expected 1 pipeline");
    TEST_ASSERT(pipelines[0].commands.size() == 1, L"Expected 1 command");
    TEST_ASSERT_EQ(pipelines[0].commands[0].program, std::wstring(L"echo"), L"Program is echo");
    TEST_ASSERT(pipelines[0].commands[0].arguments.size() == 2, L"Expected 2 arguments");
    return true;
}

static bool testParserPipeline()
{
    Lexer lexer(L"dir | findstr cpp");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    std::optional<ParseError> error;
    auto pipelines = parser.parse(error);
    TEST_ASSERT(!error, L"No parse error expected");
    TEST_ASSERT(pipelines.size() == 1, L"Expected 1 pipeline");
    TEST_ASSERT(pipelines[0].commands.size() == 2, L"Expected 2 commands in pipeline");
    TEST_ASSERT_EQ(pipelines[0].commands[0].program, std::wstring(L"dir"), L"First command is dir");
    TEST_ASSERT_EQ(pipelines[0].commands[1].program, std::wstring(L"findstr"), L"Second command is findstr");
    return true;
}

static bool testParserRedirect()
{
    Lexer lexer(L"echo hello > file.txt");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    std::optional<ParseError> error;
    auto pipelines = parser.parse(error);
    TEST_ASSERT(!error, L"No parse error expected");
    TEST_ASSERT(pipelines[0].commands[0].redirects.size() == 1, L"Expected 1 redirect");
    TEST_ASSERT_EQ(pipelines[0].commands[0].redirects[0].type, RedirectType::Output, L"Redirect type is Output");
    TEST_ASSERT_EQ(pipelines[0].commands[0].redirects[0].target, std::wstring(L"file.txt"), L"Target is file.txt");
    return true;
}

static bool testParserBackground()
{
    Lexer lexer(L"longcmd &");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    std::optional<ParseError> error;
    auto pipelines = parser.parse(error);
    TEST_ASSERT(!error, L"No parse error expected");
    TEST_ASSERT(pipelines.size() == 1, L"Expected 1 pipeline");
    TEST_ASSERT(pipelines[0].background, L"Pipeline should be background");
    return true;
}

static bool testParserSemicolons()
{
    Lexer lexer(L"echo a; echo b");
    auto tokens = lexer.tokenize();
    Parser parser(std::move(tokens));
    std::optional<ParseError> error;
    auto pipelines = parser.parse(error);
    TEST_ASSERT(!error, L"No parse error expected");
    TEST_ASSERT(pipelines.size() == 2, L"Expected 2 pipelines");
    TEST_ASSERT_EQ(pipelines[0].commands[0].program, std::wstring(L"echo"), L"First is echo");
    TEST_ASSERT_EQ(pipelines[0].commands[0].arguments[0], std::wstring(L"a"), L"Arg a");
    TEST_ASSERT_EQ(pipelines[1].commands[0].arguments[0], std::wstring(L"b"), L"Arg b");
    return true;
}

int main()
{
    std::vector<TestCase> tests = {
        {L"Lexer.SimpleCommand", testLexerSimpleCommand},
        {L"Lexer.Arguments", testLexerArguments},
        {L"Lexer.DoubleQuotes", testLexerQuotes},
        {L"Lexer.SingleQuotes", testLexerSingleQuotes},
        {L"Lexer.Escaping", testLexerEscaping},
        {L"Lexer.Pipe", testLexerPipe},
        {L"Lexer.RedirectGreater", testLexerRedirection},
        {L"Lexer.AppendRedirect", testLexerAppend},
        {L"Lexer.Background", testLexerBackground},
        {L"Lexer.Variable", testLexerVariable},
        {L"Lexer.PercentVariable", testLexerPercentVariable},
        {L"Lexer.ErrorRedirect", testLexerErrorRedirect},
        {L"Lexer.ErrorAppend", testLexerErrorAppend},
        {L"Parser.SimpleCommand", testParserCommand},
        {L"Parser.Pipeline", testParserPipeline},
        {L"Parser.Redirect", testParserRedirect},
        {L"Parser.Background", testParserBackground},
        {L"Parser.Semicolons", testParserSemicolons},
    };
    return runTests(tests);
}