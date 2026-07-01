#include <windows.h>
#include <winhttp.h>

#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#pragma comment(lib, "winhttp.lib")
#pragma execution_character_set("utf-8")

struct ChatMessage
{
    std::string role;
    std::string content;
};

void PrintChatHistory(const std::vector<ChatMessage>& chatHistory)
{
    if (chatHistory.empty())
    {
        std::cout << "저장된 대화 기록이 없습니다." << std::endl;
        return;
    }

    std::cout << "현재 메모리에 저장된 대화 기록" << std::endl;

    for (size_t i = 0; i < chatHistory.size(); i++)
    {
        const ChatMessage& message = chatHistory[i];

        if (message.role == "user")
        {
            std::cout << i + 1 << ". You: " << message.content << std::endl;
        }
        else if (message.role == "assistant")
        {
            std::cout << i + 1 << ". AI: " << message.content << std::endl;
        }
        else
        {
            std::cout << i + 1 << ". " << message.role << ": " << message.content << std::endl;
        }
    }
}

void RemoveLeadingUtf8Bom(std::string& text)
{
    const std::string utf8Bom = "\xEF\xBB\xBF";

    if (text.rfind(utf8Bom, 0) == 0)
    {
        text.erase(0, utf8Bom.size());
    }
}

std::string EscapeJsonString(const std::string& text)
{
    std::ostringstream escaped;

    for (unsigned char ch : text)
    {
        if (ch == '\"')
        {
            escaped << "\\\"";
        }
        else if (ch == '\\')
        {
            escaped << "\\\\";
        }
        else if (ch == '\n')
        {
            escaped << "\\n";
        }
        else if (ch == '\r')
        {
            escaped << "\\r";
        }
        else if (ch == '\t')
        {
            escaped << "\\t";
        }
        else if (ch < 0x20)
        {
            escaped << "\\u"
                    << std::hex << std::setw(4) << std::setfill('0')
                    << static_cast<int>(ch)
                    << std::dec;
        }
        else
        {
            escaped << ch;
        }
    }

    return escaped.str();
}

std::string BuildRequestBody(const std::string& userInput)
{
    return std::string("{")
        + "\"model\":\"qwen3:0.6b\","
        + "\"messages\":[{\"role\":\"user\",\"content\":\""
        + EscapeJsonString(userInput)
        + "\"}],"
        + "\"stream\":false"
        + "}";
}

void SkipWhitespace(const std::string& text, size_t& position)
{
    while (position < text.size())
    {
        char ch = text[position];

        if (ch != ' ' && ch != '\n' && ch != '\r' && ch != '\t')
        {
            break;
        }

        position++;
    }
}

int HexDigitToInt(char ch)
{
    if ('0' <= ch && ch <= '9')
    {
        return ch - '0';
    }

    if ('a' <= ch && ch <= 'f')
    {
        return ch - 'a' + 10;
    }

    if ('A' <= ch && ch <= 'F')
    {
        return ch - 'A' + 10;
    }

    return -1;
}

bool ReadJsonUnicodeCodePoint(const std::string& json, size_t position, unsigned int& codePoint)
{
    if (position + 4 > json.size())
    {
        return false;
    }

    codePoint = 0;

    for (int i = 0; i < 4; i++)
    {
        int value = HexDigitToInt(json[position + i]);

        if (value == -1)
        {
            return false;
        }

        codePoint = codePoint * 16 + static_cast<unsigned int>(value);
    }

    return true;
}

void AppendUtf8(std::string& text, unsigned int codePoint)
{
    if (codePoint <= 0x7F)
    {
        text += static_cast<char>(codePoint);
    }
    else if (codePoint <= 0x7FF)
    {
        text += static_cast<char>(0xC0 | ((codePoint >> 6) & 0x1F));
        text += static_cast<char>(0x80 | (codePoint & 0x3F));
    }
    else if (codePoint <= 0xFFFF)
    {
        text += static_cast<char>(0xE0 | ((codePoint >> 12) & 0x0F));
        text += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
        text += static_cast<char>(0x80 | (codePoint & 0x3F));
    }
    else if (codePoint <= 0x10FFFF)
    {
        text += static_cast<char>(0xF0 | ((codePoint >> 18) & 0x07));
        text += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
        text += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
        text += static_cast<char>(0x80 | (codePoint & 0x3F));
    }
}

bool DecodeJsonString(const std::string& json, size_t quotePosition, std::string& value, size_t& nextPosition)
{
    if (quotePosition >= json.size() || json[quotePosition] != '\"')
    {
        return false;
    }

    value = "";
    size_t position = quotePosition + 1;

    while (position < json.size())
    {
        char ch = json[position];

        if (ch == '\"')
        {
            nextPosition = position + 1;
            return true;
        }

        if (ch != '\\')
        {
            value += ch;
            position++;
            continue;
        }

        position++;

        if (position >= json.size())
        {
            return false;
        }

        char escaped = json[position];

        if (escaped == '\"')
        {
            value += '\"';
        }
        else if (escaped == '\\')
        {
            value += '\\';
        }
        else if (escaped == '/')
        {
            value += '/';
        }
        else if (escaped == 'b')
        {
            value += '\b';
        }
        else if (escaped == 'f')
        {
            value += '\f';
        }
        else if (escaped == 'n')
        {
            value += '\n';
        }
        else if (escaped == 'r')
        {
            value += '\r';
        }
        else if (escaped == 't')
        {
            value += '\t';
        }
        else if (escaped == 'u')
        {
            unsigned int codePoint = 0;

            if (!ReadJsonUnicodeCodePoint(json, position + 1, codePoint))
            {
                return false;
            }

            position += 4;

            // 이모지처럼 두 개의 \u 값으로 표현되는 문자를 처리합니다.
            if (0xD800 <= codePoint && codePoint <= 0xDBFF)
            {
                if (position + 6 < json.size() && json[position + 1] == '\\' && json[position + 2] == 'u')
                {
                    unsigned int lowSurrogate = 0;

                    if (ReadJsonUnicodeCodePoint(json, position + 3, lowSurrogate)
                        && 0xDC00 <= lowSurrogate && lowSurrogate <= 0xDFFF)
                    {
                        codePoint = 0x10000
                            + ((codePoint - 0xD800) * 0x400)
                            + (lowSurrogate - 0xDC00);
                        position += 6;
                    }
                }
            }

            AppendUtf8(value, codePoint);
        }
        else
        {
            return false;
        }

        position++;
    }

    return false;
}

bool ExtractMessageContent(const std::string& json, std::string& messageContent)
{
    messageContent = "";

    size_t messagePosition = json.find("\"message\"");

    if (messagePosition == std::string::npos)
    {
        return false;
    }

    size_t messageColon = json.find(':', messagePosition);

    if (messageColon == std::string::npos)
    {
        return false;
    }

    size_t objectStart = json.find('{', messageColon);

    if (objectStart == std::string::npos)
    {
        return false;
    }

    size_t contentPosition = json.find("\"content\"", objectStart);

    if (contentPosition == std::string::npos)
    {
        return false;
    }

    size_t contentColon = json.find(':', contentPosition);

    if (contentColon == std::string::npos)
    {
        return false;
    }

    size_t valueStart = contentColon + 1;
    SkipWhitespace(json, valueStart);

    size_t nextPosition = 0;
    return DecodeJsonString(json, valueStart, messageContent, nextPosition);
}

void CloseWinHttpHandle(HINTERNET handle)
{
    if (handle != NULL)
    {
        WinHttpCloseHandle(handle);
    }
}

bool SendMessageToOllama(const std::string& userInput, std::string& response, std::string& errorMessage)
{
    response = "";
    errorMessage = "";

    std::string requestBody = BuildRequestBody(userInput);

    HINTERNET session = WinHttpOpen(
        L"LocalAIChat/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );

    if (session == NULL)
    {
        errorMessage = "WinHttpOpen 실패. 오류 코드: " + std::to_string(GetLastError());
        return false;
    }

    HINTERNET connection = WinHttpConnect(session, L"localhost", 11434, 0);

    if (connection == NULL)
    {
        errorMessage = "WinHttpConnect 실패. Ollama가 실행 중인지 확인하세요. 오류 코드: " + std::to_string(GetLastError());
        CloseWinHttpHandle(session);
        return false;
    }

    HINTERNET request = WinHttpOpenRequest(
        connection,
        L"POST",
        L"/api/chat",
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        0
    );

    if (request == NULL)
    {
        errorMessage = "WinHttpOpenRequest 실패. 오류 코드: " + std::to_string(GetLastError());
        CloseWinHttpHandle(connection);
        CloseWinHttpHandle(session);
        return false;
    }

    std::wstring headers = L"Content-Type: application/json; charset=utf-8\r\n";

    BOOL sendResult = WinHttpSendRequest(
        request,
        headers.c_str(),
        static_cast<DWORD>(-1L),
        const_cast<char*>(requestBody.c_str()),
        static_cast<DWORD>(requestBody.size()),
        static_cast<DWORD>(requestBody.size()),
        0
    );

    if (sendResult == FALSE)
    {
        errorMessage = "WinHttpSendRequest 실패. Ollama API 요청을 보낼 수 없습니다. 오류 코드: " + std::to_string(GetLastError());
        CloseWinHttpHandle(request);
        CloseWinHttpHandle(connection);
        CloseWinHttpHandle(session);
        return false;
    }

    BOOL receiveResult = WinHttpReceiveResponse(request, NULL);

    if (receiveResult == FALSE)
    {
        errorMessage = "WinHttpReceiveResponse 실패. Ollama 응답을 받을 수 없습니다. 오류 코드: " + std::to_string(GetLastError());
        CloseWinHttpHandle(request);
        CloseWinHttpHandle(connection);
        CloseWinHttpHandle(session);
        return false;
    }

    DWORD statusCode = 0;
    DWORD statusCodeSize = sizeof(statusCode);

    WinHttpQueryHeaders(
        request,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX,
        &statusCode,
        &statusCodeSize,
        WINHTTP_NO_HEADER_INDEX
    );

    DWORD bytesAvailable = 0;

    do
    {
        bytesAvailable = 0;

        if (WinHttpQueryDataAvailable(request, &bytesAvailable) == FALSE)
        {
            errorMessage = "WinHttpQueryDataAvailable 실패. 응답 크기를 확인할 수 없습니다. 오류 코드: " + std::to_string(GetLastError());
            CloseWinHttpHandle(request);
            CloseWinHttpHandle(connection);
            CloseWinHttpHandle(session);
            return false;
        }

        if (bytesAvailable > 0)
        {
            std::string buffer(bytesAvailable, '\0');
            DWORD bytesRead = 0;

            if (WinHttpReadData(request, &buffer[0], bytesAvailable, &bytesRead) == FALSE)
            {
                errorMessage = "WinHttpReadData 실패. 응답을 읽을 수 없습니다. 오류 코드: " + std::to_string(GetLastError());
                CloseWinHttpHandle(request);
                CloseWinHttpHandle(connection);
                CloseWinHttpHandle(session);
                return false;
            }

            buffer.resize(bytesRead);
            response += buffer;
        }
    } while (bytesAvailable > 0);

    CloseWinHttpHandle(request);
    CloseWinHttpHandle(connection);
    CloseWinHttpHandle(session);

    if (statusCode != 200)
    {
        errorMessage = "Ollama API가 HTTP 상태 코드 " + std::to_string(statusCode) + " 를 반환했습니다.";
        return false;
    }

    return true;
}

int main()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    // 프로그램이 실행되는 동안 대화 기록을 메모리에 저장하는 목록입니다.
    std::vector<ChatMessage> chatHistory;
    std::string userInput;

    std::cout << "Local AI Chat - Step 5" << std::endl;
    std::cout << "Ollama 로컬 LLM API와 연결하는 콘솔 프로그램입니다." << std::endl;
    std::cout << "실행 전에 Ollama와 qwen3:0.6b 모델이 켜져 있어야 합니다." << std::endl;
    std::cout << "종료하려면 /exit 을 입력하세요." << std::endl;
    std::cout << "대화 기록을 보려면 /history 를 입력하세요." << std::endl;

    while (true)
    {
        std::cout << std::endl;
        std::cout << "입력: ";

        std::getline(std::cin, userInput);
        RemoveLeadingUtf8Bom(userInput);

        if (userInput == "")
        {
            std::cout << "빈 입력입니다. 다시 입력해주세요." << std::endl;
            continue;
        }

        if (userInput == "/exit")
        {
            std::cout << "프로그램을 종료합니다." << std::endl;
            break;
        }

        if (userInput == "/history")
        {
            PrintChatHistory(chatHistory);
            continue;
        }

        std::cout << "You: " << userInput << std::endl;
        std::cout << "AI 응답 요청 중..." << std::endl;

        std::string response;
        std::string errorMessage;

        bool success = SendMessageToOllama(userInput, response, errorMessage);

        if (success)
        {
            std::string messageContent;

            if (ExtractMessageContent(response, messageContent))
            {
                std::cout << "AI: " << messageContent << std::endl;

                ChatMessage userMessage;
                userMessage.role = "user";
                userMessage.content = userInput;

                ChatMessage assistantMessage;
                assistantMessage.role = "assistant";
                assistantMessage.content = messageContent;

                chatHistory.push_back(userMessage);
                chatHistory.push_back(assistantMessage);
            }
            else
            {
                std::cout << "오류: message.content 값을 찾지 못했습니다." << std::endl;
                std::cout << "디버깅용 전체 JSON: " << response << std::endl;
            }
        }
        else
        {
            std::cout << "오류: " << errorMessage << std::endl;
            std::cout << "Ollama가 실행 중인지, qwen3:0.6b 모델이 준비되어 있는지 확인하세요." << std::endl;
        }
    }

    return 0;
}
