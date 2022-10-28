#pragma once

#include <iostream>
#include <functional>
#include <string>
#include <windows.h>
#include "trialfuncs.hpp"
#include <thread>
#include <syncstream>

template<typename T>
class ResultAndAttempts 
{
public:
	std::variant<os::lab1::compfuncs::hard_fail, os::lab1::compfuncs::soft_fail, T> result;
	int attemtps;
};

class Manager 
{
public:
	template<typename T>
	void runInterface()
	{
		HANDLE hMutex;
		hMutex = CreateMutex(NULL, FALSE, TEXT("myMutex"));

		if (hMutex == NULL)
		{
			std::cout << "CreateMutex error: " << GetLastError() << std::endl;
		}
		else
		{
			if (GetLastError() == ERROR_ALREADY_EXISTS)
			{
				std::cout << "CreateMutex opened an existing mutex\n";
			}
		}

		int attemptsF = 0, attemptsG = 0, x;
		std::variant<os::lab1::compfuncs::hard_fail, os::lab1::compfuncs::soft_fail, T> compResultF, compResultG;

		std::cout << "Enter x: ";
		std::cin >> x;

		std::thread threadF([this, &compResultF, x, &attemptsF] {startProcess<T>(x,"f",compResultF,attemptsF); });
		std::thread threadG([this, &compResultG, x, &attemptsG] {startProcess<T>(x,"g",compResultG,attemptsG); });

		threadF.join();
		threadG.join();

		std::cout << "\nRESULT OF COMPUTATION:\n";
		if (attemptsF < 0 || attemptsG < 0) 
		{
			output<T>(compResultF, compResultG,attemptsF, attemptsG);
			CloseHandle(hMutex);
			return;
		}
		if (!output<T>(compResultF, compResultG,attemptsF,attemptsG))
		{
			std::cout << DOUBLE_MULT(std::get<2>(compResultF), std::get<2>(compResultG)) << std::endl;
		}

		CloseHandle(hMutex);
	}

	double DOUBLE_MULT(double f, double g) 
	{
		std::cout << "f() * g(): ";
		return f * g;
	}

	template<typename T>
	bool output(std::variant<os::lab1::compfuncs::hard_fail, os::lab1::compfuncs::soft_fail, T>resultF, 
		std::variant<os::lab1::compfuncs::hard_fail, os::lab1::compfuncs::soft_fail, T>resultG,int attemptsF,int attemptsG) 
	{
		std::string nameF = "f", nameG = "g";

		if (std::holds_alternative<os::lab1::compfuncs::soft_fail>(resultF) || std::holds_alternative<os::lab1::compfuncs::soft_fail>(resultG)
			|| std::holds_alternative<os::lab1::compfuncs::hard_fail>(resultF) || std::holds_alternative<os::lab1::compfuncs::hard_fail>(resultG))
		{
			std::cout << nameF + "(): " << resultF <<std::endl;
			if (std::holds_alternative<os::lab1::compfuncs::soft_fail>(resultF))
			{
				if (attemptsF < 0)
				{
					attemptsF = -attemptsF;
					std::cout << "function " <<  nameF <<"() was canceled\n";
				}
				std::cout << nameF + "(): " << "tryed " << attemptsF << " time/s" << std::endl;
			}
			std::cout << nameG + "(): " << resultG << std::endl;
			if (std::holds_alternative<os::lab1::compfuncs::soft_fail>(resultG))
			{
				if (attemptsG < 0)
				{
					attemptsG = -attemptsG;
					std::cout << "function " << nameG << "() was canceled\n";
				}
				std::cout << nameG + "(): " << "tryed " << attemptsG << " time/s" << std::endl;
			}
			return true;
		}
		
		return false;
	}

	template<typename T>
	void startProcess(int numberParam, std::string functionParam,std::variant<os::lab1::compfuncs::hard_fail, os::lab1::compfuncs::soft_fail, T>&compResult, int &numOfAttempts) 
	{
		std::string tempstr(functionParam + " " + std::to_string(numberParam));
		const char* tempchr = tempstr.c_str();
		TCHAR paramToPassInProces[4];
		swprintf(paramToPassInProces, 4, L"%hs", tempchr);

		HANDLE myPipe;
		STARTUPINFO si1;
		PROCESS_INFORMATION pi1;
		ZeroMemory(&si1, sizeof(si1));
		si1.cb = sizeof(si1);
		ZeroMemory(&pi1, sizeof(pi1));

		if (!CreateProcess(L"C:\\OSLab1\\client\\Debug\\client.exe", paramToPassInProces, NULL, NULL, FALSE, 0, NULL, NULL, &si1, &pi1))
		{
			std::osyncstream(std::cout) << "CreateProcess failed " << GetLastError() << std::endl;
			return;
		}

		if (functionParam == "f") 
		{
			myPipe = CreateNamedPipe(L"\\\\.\\pipe\\functionFpipe", PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 1024 * 16, 1024 * 16, NMPWAIT_USE_DEFAULT_WAIT, NULL);
		}
		else
		{
			myPipe = CreateNamedPipe(L"\\\\.\\pipe\\functionGpipe", PIPE_ACCESS_DUPLEX, PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 1024 * 16, 1024 * 16, NMPWAIT_USE_DEFAULT_WAIT, NULL);
		}

		if (myPipe == NULL || myPipe == INVALID_HANDLE_VALUE) {
			std::osyncstream(std::cout) << "Failed to create outbound pipe instance\n";
			return;
		}
		BOOL result = ConnectNamedPipe(myPipe, NULL);
		if (!result) 
		{
			std::osyncstream(std::cout) << "Failed to make connection on named pipe\n";
			CloseHandle(myPipe);
			return;
		}
		DWORD numBytesRead1 = 0;
		ResultAndAttempts<T> tempVar;
		result = ReadFile(myPipe, &tempVar, sizeof(tempVar), &numBytesRead1, NULL);
		if (result) 
		{
			compResult = tempVar.result;
			numOfAttempts = tempVar.attemtps;
		}
		else 
		{
			std::osyncstream(std::cout) << "Failed to read data from the pipe\n";
		}

		CloseHandle(myPipe);
		WaitForSingleObject(pi1.hProcess, INFINITE);
		CloseHandle(pi1.hProcess);
		CloseHandle(pi1.hThread);
	}
};






