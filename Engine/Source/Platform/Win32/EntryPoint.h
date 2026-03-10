#pragma once
#include "Platform/Win32/Application.h"

extern Cauda::Application* Cauda::CreateApplication();

#ifdef _DISTRIB

int APIENTRY WinMain(HINSTANCE, HINSTANCE , PSTR, int )
{
	auto App = Cauda::CreateApplication();

	App->Run();

	delete App;

	return 0;
}
#else 
int main(int argc, char* argv[])
{
	auto App = Cauda::CreateApplication();

	App->Run();

	delete App;

	return 0;
}
#endif