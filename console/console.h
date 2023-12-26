#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include <stdint.h>
#include <stdbool.h>

#define CONSOLE_MAX_COLUMN 80
#define CONSOLE_MAX_ROW 25
#define CONSOLE_MAX_SCROLLBACK (CONSOLE_MAX_ROW)

#define CONSOLE_MAX_COMMAND_HISTORY 30

#define CONSOLE_MAX_COMMAND_NAME CONSOLE_MAX_COLUMN
#define CONSOLE_MAX_COMMANDS 100

typedef struct Console_s Console_t;

typedef struct
{
	char commandName[CONSOLE_MAX_COMMAND_NAME];
	void (*commandFunction)(Console_t *, char *);
} ConsoleCommand_t;

typedef struct Console_s
{
	struct {
		char text[CONSOLE_MAX_COLUMN];
	} buffer[CONSOLE_MAX_SCROLLBACK];

	int32_t newLine, column, viewBottom;

	char commandHistory[CONSOLE_MAX_COMMAND_HISTORY][CONSOLE_MAX_COMMAND_NAME];
	int32_t numCommandHistory;

	int32_t newCommand, currentCommand;

	ConsoleCommand_t commands[CONSOLE_MAX_COMMANDS];
	uint32_t numCommand;

	uint32_t width, height;

	bool active;
} Console_t;

bool Console_ExecFile(Console_t *console, char *filename);
void Console_ClearCommandHistory(Console_t *console);
void Console_Clear(Console_t *console);
void Console_Init(Console_t *console, uint32_t width, uint32_t height);
void Console_Destroy(Console_t *console);
void Console_Advance(Console_t *console);
void Console_Scroll(Console_t *console, bool up);
void Console_Out(Console_t *console, char *string);
bool Console_AddCommand(Console_t *console, char *commandName, void (*commandFunction)(Console_t *, char *));
bool Console_ExecCommand(Console_t *console, char *command, char *param);
void Console_History(Console_t *console, bool up);
void Console_Backspace(Console_t *console);
void Console_Process(Console_t *console);
void Console_KeyInput(Console_t *console, uint32_t keyCode);
void Console_Draw(Console_t *console);

#endif
