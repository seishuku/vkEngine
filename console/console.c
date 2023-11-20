#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../font/font.h"
#include "console.h"

#ifdef WIN32
#include <string.h>
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

extern Font_t Fnt;

// Standard/basic console commands
void Console_CmdClear(Console_t *Console, char *Param)
{
	Console_Clear(Console);
}

void Console_CmdEcho(Console_t *Console, char *Param)
{
	Console_Out(Console, Param);
}

bool Console_ExecFile(Console_t *Console, char *Filename)
{
	FILE *Stream=NULL;
	char Buf[CONSOLE_MAX_COLUMN];

	if(!Filename)
	{
		Console_Out(Console, "No file specified");
		return false;
	}

	Stream=fopen(Filename, "r");

	if(Stream==NULL)
	{
		sprintf(Buf, "Unable to open file: %s", Filename);
		Console_Out(Console, Buf);

		return false;
	}

	while(fgets(Buf, CONSOLE_MAX_COLUMN, Stream))
	{
		strtok(Buf, " \n\f");
		char *Param=Buf+strlen(Buf)+1;

		if(Param[0]==0)
			Param=NULL;

		if(!Console_ExecCommand(Console, Buf, Param))
			Console_Out(Console, "Invalid command in file");
	}

	fclose(Stream);

	return true;
}

void Console_ClearCommandHistory(Console_t *Console)
{
	memset(Console->CommandHistory, '\0', sizeof(char)*CONSOLE_MAX_COMMAND_HISTORY*CONSOLE_MAX_COMMAND_NAME);
	Console->NumCommandHistory=0;

	Console->NewCommand=0;
	Console->CurrentCommand=0;
}

void Console_Clear(Console_t *Console)
{
	memset(Console->Buffer, '\0', sizeof(char)*CONSOLE_MAX_ROW*CONSOLE_MAX_COLUMN);

	Console->NewLine=0;
	Console->ViewBottom=0;
}

void Console_Init(Console_t *Console, uint32_t Width, uint32_t Height)
{
	Console->Column=1;

	Console->Width=Width;
	Console->Height=Height;

	Console->NumCommand=0;

	Console_AddCommand(Console, "echo", Console_CmdEcho);
	Console_AddCommand(Console, "clear", Console_CmdClear);
//	Console_AddCommand(Console, "exec", Console_ExecFile);

	Console_Clear(Console);
	Console_ClearCommandHistory(Console);
	Console_Advance(Console);

	Console->Buffer[Console->NewLine].Text[0]=']';
	Console->Buffer[Console->NewLine].Text[1]='_';
	Console->Buffer[Console->NewLine].Text[2]='\0';
}

void Console_Destroy(Console_t *Console)
{
}

void Console_Advance(Console_t *Console)
{
	Console->NewLine--;

	if(Console->NewLine<0)
		Console->NewLine+=CONSOLE_MAX_SCROLLBACK;

	Console->Buffer[Console->NewLine].Text[0]='\0';
	Console->ViewBottom=Console->NewLine;
}

void Console_Scroll(Console_t *Console, bool Up)
{
	if(Up)
	{
		Console->ViewBottom++;

		if(Console->ViewBottom>=CONSOLE_MAX_SCROLLBACK+CONSOLE_MAX_SCROLLBACK)
			Console->ViewBottom-=CONSOLE_MAX_SCROLLBACK+CONSOLE_MAX_SCROLLBACK;

		if(Console->ViewBottom==Console->NewLine)
			Console->ViewBottom--;
	}
	else
	{
		Console->ViewBottom--;

		if(Console->ViewBottom<0)
			Console->ViewBottom+=CONSOLE_MAX_SCROLLBACK;

		if(Console->ViewBottom==Console->NewLine-1)
			Console->ViewBottom=Console->NewLine;
	}
}

void Console_Out(Console_t *Console, char *string)
{
	Console_Advance(Console);

	if(string)
		strcpy(Console->Buffer[Console->NewLine].Text, string);
}

bool Console_AddCommand(Console_t *Console, char *CommandName, void (*CommandFunction)(Console_t *, char *))
{
	if(Console==NULL)
		return false;

	if(Console->NumCommand>=CONSOLE_MAX_COMMANDS)
		return false;

	strcpy(Console->Commands[Console->NumCommand].CommandName, CommandName);
	Console->Commands[Console->NumCommand].CommandFunction=CommandFunction;

	Console->NumCommand++;

	return true;
}

bool Console_ExecCommand(Console_t *Console, char *Command, char *Param)
{
	if(Console==NULL)
		return false;

	if(Command==NULL)
		return false;

	// Search through command list and run it
	for(uint32_t i=0;i<Console->NumCommand;i++)
	{
		if(strcasecmp(Command, Console->Commands[i].CommandName)==0)
		{
			Console->Commands[i].CommandFunction(Console, Param);
			return true;
		}
	}

	return false;
}

void Console_History(Console_t *Console, bool Up)
{
	if(Up)
	{
		Console->CurrentCommand++;

		if(Console->CurrentCommand>Console->NumCommandHistory)
			Console->CurrentCommand=Console->NumCommandHistory;
	}
	else
	{
		Console->CurrentCommand--;

		if(Console->CurrentCommand<0)
			Console->CurrentCommand=0;
	}

	if(Console->CurrentCommand!=-1)
	{
		char buf[CONSOLE_MAX_COLUMN];

		buf[0]=']';
		buf[1]='\0';

		int32_t CommandNumber=Console->NewCommand-Console->CurrentCommand;

		if(CommandNumber<0)
			CommandNumber+=Console->NumCommandHistory;

		strcat(buf, Console->CommandHistory[CommandNumber]);
		strcat(buf, "_");
		strcpy(Console->Buffer[Console->NewLine].Text, buf);

		Console->Column=(int)strlen(buf)-1;
	}
}

void Console_Backspace(Console_t *Console)
{
	// Backspace only up to the prompt character
	if(Console->Column>1)
	{
		Console->Buffer[Console->NewLine].Text[Console->Column]='\0';
		Console->Buffer[Console->NewLine].Text[Console->Column-1]='_';
		Console->Column--;
	}
}

void Console_Process(Console_t *Console)
{
	// Terminate the line and process it
	Console->Buffer[Console->NewLine].Text[Console->Column]='\0';

	char Command[CONSOLE_MAX_COLUMN];

	// Copy the line out of the console buffer into a command buffer (not including the prompt character)
	strcpy(Command, Console->Buffer[Console->NewLine].Text+1);

	// Also place the command into the command history
	strcpy(Console->CommandHistory[Console->NewCommand], Command);

	Console->NewCommand++;
	Console->NewCommand%=CONSOLE_MAX_COMMAND_HISTORY;
	Console->CurrentCommand=0;

	Console->NumCommandHistory++;

	if(Console->NumCommandHistory>CONSOLE_MAX_COMMAND_HISTORY)
		Console->NumCommandHistory=CONSOLE_MAX_COMMAND_HISTORY;

	// Find the parameter after the command
	char *CommandParam=strrchr(Command, ' ');

	// If there was a parameter to the command
	if(CommandParam!=NULL)
	{
		// Replace the space with a null-terminator and advance to split the string
		//     around the space and separate command and parameter.
		*CommandParam='\0';
		CommandParam++;
	}

	if(!Console_ExecCommand(Console, Command, CommandParam))
		Console_Out(Console, "Invalid command");

	// New line, prompt and cursor
	Console_Advance(Console);

	Console->Column=1;
	Console->Buffer[Console->NewLine].Text[0]=']';
	Console->Buffer[Console->NewLine].Text[1]='_';
	Console->Buffer[Console->NewLine].Text[2]='\0';
}

void Console_KeyInput(Console_t *Console, uint32_t KeyCode)
{
	// Don't type past edge of console
	if(Console->Column>(signed)Console->Width-2)
		return;

	Console->Buffer[Console->NewLine].Text[Console->Column++]=KeyCode;
	Console->Buffer[Console->NewLine].Text[Console->Column]='_';
	Console->Buffer[Console->NewLine].Text[Console->Column+1]='\0';
}

void Console_Draw(Console_t *Console)
{
	if(!Console->Active)
		return;

	uint32_t temp=Console->ViewBottom;

	for(uint32_t i=0;i<Console->Height;i++)
	{
		Font_Print(&Fnt, 16.0f, 0.0f, (float)i*16.0f, "%s", Console->Buffer[temp].Text);
		temp=(temp+1)%CONSOLE_MAX_ROW;
	}
}
