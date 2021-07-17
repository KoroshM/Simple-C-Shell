/** @file shell.c
 * @author Korosh Moosavi
 * @date 2021-04-14
 *
 * shell.c file:
 * This file creates a very basic C shell for Unix systems.
 * The shell supports basic commands and can either pipe or redirect
 *   outputs.
 *   Only one special case can be handled at a time.
 * History command only stores the last command entered, valid or not.
 *   It will not store a history command.
 * The shell has all the same limitations as the terminal it is being
 *   run on, so for example outputs from commands using & will result
 *   in scrambled formatting.
 * There's a known bug where using & can occasionally result in a blank
 *   command line on the current or next command input.
 *   This is just a display issue, the program is still running and
 *     functioning as normal.
 *   Just press Enter to restore the "osh>" prompt
 * 
 * Assumptions:
 * Only one special handler will be used at a time (not counting &)
 * This means that the command can contain either an I/O redirect or
 *   a pipe, but not both.
 * Data in existing output files are OK to be overwritten, or are
 *   otherwise backed up (file will be cleared before it receives output)
 * & will only be included as the final character in an input
 *   (no & on the first process of a pipe, e.g. ls & | wc)
 */
#include <fcntl.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LINE 80 /* The maximum length command */


/** -------------------------------- main -------------------------------------
 * The only method used for this program is main()
 * The shell functions by accepting a user input and forking with the child
 *   calling execvp() using the entered text as tokenized arguments
 *   where args[0] is the command to be executed
 * 
 *
 * Assumptions:
 * A space must be included between each argument or separate character
 *   i.e. ls|wc will not work, but ls | wc will
 * If outputting to an existing file the data is okay to be erased or is 
 *   backed up, as this program will erase contents before outputting
 * There are no extra characters following the arguments
 *   i.e. ps -a\n
 */
int main(void)
{
   enum { READ, WRITE };

   int should_run = 1; /* flag to determine when to exit program */
   int argSize = MAX_LINE / 2 + 1;
   char *args[argSize]; /* command line arguments */
   char history[MAX_LINE] = {'\0'};

   printf("Unix C Shell by Korosh Moosavi. Begin typing commands, or type \"exit\" to quit.\n");

   while (should_run)
   {
      char theCommand[MAX_LINE];          // The entered command
      int numArgs = 0;                    // # arguments included
      int bgProcess = 0;                  // Flag for &

      printf("osh> ");                    // Print shell line starter
      fflush(stdout);                     // Flush output

      fgets(theCommand, sizeof(theCommand), stdin);   // Get command
      theCommand[strlen(theCommand) - 1] = '\0';      // Remove \n

      // Empty input
      if (theCommand[0] == '\0') 
      {
         continue;
      }

      // Check for exit
      if (strcmp(theCommand, "exit") == 0)
      {
         should_run = 0;
         continue;
      }

      // Check for history (!!)
      if (strcmp(theCommand, "!!") == 0)  // History call
      {
         if (history[0] == '\0')          // No valid previous command
         {
            printf("No command in history.\n");
            continue;
         }
         else                             // Reuse last command
         {
            strcpy(theCommand, history);  // Copy command from history
            printf("Previous command: %s\n", theCommand);
         }
      }
      
      // New command
      else 
      {
         strcpy(history, theCommand);     // Copy command to history
      }
      
      // Check for background process (&)
      bgProcess = theCommand[strlen(theCommand) - 1] == '&'; 
      if (bgProcess)                      // Remove & from arguments
      {
         theCommand[strlen(theCommand) - 1] = '\0';
      }

      // Tokenize arguments
      args[numArgs] = strtok(theCommand, " ");
      while (args[numArgs] != NULL)
      {
         numArgs++;                       // Save each token to its own index
         args[numArgs] = strtok(NULL, " ");
      }

      // Find special case characters > < |
      //  Only finds 1
      //  If there are multiple only the first will be found
      //    The rest are passed as arguments
      int needPipe = 0;    // 0 = no pipe      1 = needs pipe
      int ioRedirect = 0;  // 0 = no redirect  1 = input      2 = output
      int i;               // Index of special case character, if found
      for (i = 1; i <= numArgs - 1; i++)
      {
         if (strcmp(args[i], "<") == 0)      // Input redirect
         {
            ioRedirect = 1;
            break;
         }
         else if (strcmp(args[i], ">") == 0) // Output redirect
         {
            ioRedirect = 2;
            break;
         }
         else if (strcmp(args[i], "|") == 0) // Pipe needed
         {
            needPipe = 1;
            break;
         }
      }

      // Begin forking
      int status = 0;
      pid_t childPid= fork();
      
      if (childPid > 0)       // -------------------------------------- Parent
      {
         if (bgProcess == 0)  // Parent waits if command didn't have an &
         {
            wait(&status);
         }
      }
      
      else if (childPid == 0) // -------------------------------------- Child
      { 
         // Pipe between child and grandchild
         if (needPipe) 
         {
            int fd1[2];
            int status2;

            if (pipe(fd1) == -1)                      // Make a pipe
            {
               perror("Pipe failed");
               exit(1);
            }

            // Split command into its sub-commands
            char *firstCommand[i + 1];                // First process
            char *secondCommand[numArgs - i];         // Second process

            for (int j = 0; j < i; j++)               // Copy first command
            {
               firstCommand[j] = args[j];
            }
            firstCommand[i] = NULL;                   // Set null terminator

            for (int j = 0; j < numArgs - i - 1; j++) // Copy second command
            {
               secondCommand[j] = args[j + i + 1];
            }
            secondCommand[numArgs - i - 1] = NULL;    // Set null termintator

            
            // Fork and execute commands separately
            int gChildPid = fork(); 
            
            if (gChildPid > 0)      // --------------------------- Child
            {
               wait(&status2);      // Child waits for grandchild
               dup2(fd1[READ], STDIN_FILENO);
               close(fd1[READ]);    // then executes command + reads from pipe
               close(fd1[WRITE]);

               if (execvp(secondCommand[0], secondCommand) == -1)
               {
                  perror("Exec failed");
                  exit(1);
               }
            }

            else if (gChildPid == 0)// --------------------------- Grandchild
            {
               dup2(fd1[WRITE], STDOUT_FILENO);
               close(fd1[WRITE]);   // Grandchild begins execution of first
               close(fd1[READ]);    //   command and writes output to pipe

               if (execvp(firstCommand[0], firstCommand) == -1)
               {
                  perror("Exec failed");
                  exit(1);
               }
               exit(1);
            }

            else                    // gChildPid < 0
            {
               perror("Child fork failed");
            }
         }

         // No pipe needed
         else {
            int fd2;
            // Input redirect
            if (ioRedirect == 1)
            {
               // Open file input
               fd2 = open(args[i + 1], O_RDONLY, 0666);
               if (fd2 == -1 || args[i + 1] == NULL)
               {
                  perror("Input file failed");
                  exit(1);
               }

               args[i] = NULL;      // Remove <
               args[i + 1] = NULL;  // Remove input file name
               dup2(fd2, STDIN_FILENO);
            }

            // Output redirect
            else if (ioRedirect == 2)
            {
               // Open file output
               // This will clear any contents inside the file before outputting
               fd2 = open(args[i + 1], O_WRONLY | O_CREAT | O_TRUNC, 0666);
               if (fd2 == -1 || args[i + 1] == NULL)
               {
                  perror("Output file failed");
                  exit(1);
               }

               args[i] = NULL;      // Remove >
               args[i + 1] = NULL;  // Remove output file name
               dup2(fd2, STDOUT_FILENO);
            }

            // Execute command
            if (execvp(args[0], args) == -1)
            {
               perror("Exec failed");
               exit(1);
            }

            switch(ioRedirect){
               case 1 :             // Input was redirected
                  close(STDIN_FILENO);
                  break;
               
               case 2 :             // Output was redirected
                  close(STDOUT_FILENO);
                  break;
            }
            close(fd2);             // Close file
         }
      }

      else
      {
         perror("Fork failed");     // childPid < 0
      }
   }
}
