#include <iostream>
#include <string>

int main()
{
    // 사용자가 입력한 한 줄 전체를 저장할 변수입니다.
    std::string userInput;

    std::cout << "Local AI Chat - Step 2" << std::endl;
    std::cout << "아직 로컬 LLM과 연결되지 않은 기본 채팅 프로그램입니다." << std::endl;
    std::cout << "종료하려면 /exit 을 입력하세요." << std::endl;

    while (true)
    {
        std::cout << std::endl;
        std::cout << "입력: ";

        // getline은 공백을 포함한 한 줄 전체를 입력받습니다.
        std::getline(std::cin, userInput);

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

        std::cout << "You: " << userInput << std::endl;
        std::cout << "AI: 아직 로컬 LLM과 연결되지 않았습니다." << std::endl;
    }

    return 0;
}