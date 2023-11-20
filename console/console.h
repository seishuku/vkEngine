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
	char CommandName[CONSOLE_MAX_COMMAND_NAME];
	void (*CommandFunction)(Console_t *, char *);
} ConsoleCommand_t;

typedef struct Console_s
{
	struct {
		char Text[CONSOLE_MAX_COLUMN];
	} Buffer[CONSOLE_MAX_SCROLLBACK];

	int32_t NewLine, Column, ViewBottom;

	char CommandHistory[CONSOLE_MAX_COMMAND_HISTORY][CONSOLE_MAX_COMMAND_NAME];
	int32_t NumCommandHistory;

	int32_t NewCommand, CurrentCommand;

	ConsoleCommand_t Commands[CONSOLE_MAX_COMMANDS];
	uint32_t NumCommand;

	uint32_t Width, Height;

	bool Active;
} Console_t;

bool Console_ExecFile(Console_t *Console, char *Filename);
void Console_ClearCommandHistory(Console_t *Console);
void Console_Clear(Console_t *Console);
void Console_Init(Console_t *Console, uint32_t Width, uint32_t Height);
void Console_Destroy(Console_t *Console);
void Console_Advance(Console_t *Console);
void Console_Scroll(Console_t *Console, bool Up);
void Console_Out(Console_t *Console, char *string);
bool Console_AddCommand(Console_t *Console, char *CommandName, void (*CommandFunction)(Console_t *, char *));
bool Console_ExecCommand(Console_t *Console, char *Command, char *Param);
void Console_History(Console_t *Console, bool Up);
void Console_Backspace(Console_t *Console);
void Console_Process(Console_t *Console);
void Console_KeyInput(Console_t *Console, uint32_t KeyCode);
void Console_Draw(Console_t *Console);

#endif
