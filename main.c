#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/signal.h>
#include <termios.h>
#include <limits.h>
#include <unistd.h>
#include <pwd.h>
#include "term.h"

#define SHELL_LINE_MAX 2048
#define SHELL_TOKEN_MAX 1024

pid_t pid;
static int clear_next_line = 0;

void signal_handler(int signal) {
  switch (signal) {
    case SIGCHLD:
      waitpid(pid, NULL, WNOHANG);
      break;
    case SIGINT:
      if (pid > 0 && kill(pid,SIGTERM) == 0){
        clear_next_line = 0;
        write(STDOUT_FILENO, "\n", 1);
      } else {
        clear_next_line = 1;
      }
      break;
  }
}

void setup_signal_handlers() {
  if (isatty(STDIN_FILENO)) {
    signal(SIGINT, signal_handler);
    signal(SIGCHLD, signal_handler);
  } else {
    exit(-1);
  }
}

void run(char **args) {
  if ((pid = fork()) == 0) {
    tdisable();
    signal(SIGINT, SIG_IGN);
    if (execvp(*args, args) == -1) {
      printf("command not found: %s\n", *args);
      kill(getpid(), SIGTERM);
    }
  } else {
    waitpid(-1, NULL, 0);
    tenable();
  }
}

typedef enum {
  COMMAND_TYPE_OTHER,
  COMMAND_TYPE_CD,
  COMMAND_TYPE_EXIT
} COMMAND_TYPE;

COMMAND_TYPE get_command_type(const char *command) {
  if (strcmp(command, "exit") == 0)
    return COMMAND_TYPE_EXIT;
  if (strcmp(command, "cd") == 0)
    return COMMAND_TYPE_CD;
  return COMMAND_TYPE_OTHER;
}

char *get_home_dir(void) {
  struct passwd *pw = getpwuid(geteuid());
  if (pw != NULL) {
    return pw->pw_dir;
  }
  return "";
}

char *get_user_name(void) {
  struct passwd *pw = getpwuid(geteuid());
  if (pw != NULL) {
    return pw->pw_name;
  }
  return "";
}

char *strrep(const char *_is, const char *mat, const char *rep, int max_count) {
  char *is = strdup(_is);
  size_t replen = strlen(rep),
         matlen = strlen(mat),
         islen = strlen(is);
  for (size_t i = 0, count = 0; i < islen; i++) {
    if (strncmp(is + i, mat, matlen) == 0) {
      is = realloc(is, (islen + (replen - matlen) + 1) * sizeof(char));
      memmove(is + i + replen, is + i + matlen, islen - i - matlen);
      memcpy(is + i, rep, replen);
      i += replen - 1;
      islen += replen;
      is[islen - 1] = '\0';
      if (++count == max_count)
        break;
    }
  }
  return is;
}

void handle(int argc, char **args) {
  switch (get_command_type(*args)) {
    case COMMAND_TYPE_EXIT:
      exit(0);
      break;
    case COMMAND_TYPE_CD:
      {
        if (argc > 2) {
          printf("cd: unexpected argument: %s\n", args[2]);
        } else {
          if (argc != 2) {
            chdir(get_home_dir());
            break;
          }
          char *path = strdup(args[1]);
          char *expanded_path = strrep(path, "~", get_home_dir(), -1);
          if (chdir(expanded_path)) {
            printf("cd: no such file or directory: %s\n", expanded_path);
          }
          free(expanded_path);
          free(path);
        }
        break;
      }
    default:
      if (strcmp(*args, "ls") == 0) {
        args = realloc(args, (argc + 2) * sizeof(char*));
        args[argc] = malloc(20);
        strcpy(args[argc], "--color=auto\0");
        run(args);
        free(args[argc]);
      } else {
        run(args);
      }
      break;
  }
}

void prompt(){
  char hn[HOST_NAME_MAX];
  char *un = get_user_name();
  gethostname(hn, HOST_NAME_MAX);
  printf("(%s@%s) -> ", un, hn);
}

typedef struct {
  char **t;
  size_t l;
} line_token_t;

void free_line(line_token_t *line) {
  for (size_t i = 0; i < line->l; i++) {
    free(line->t[i]);
  }
  free(line);
}

line_token_t *split_line_tokens(char *line) {
  line_token_t *lt = malloc(sizeof(line_token_t));
  lt->l = 0;
  lt->t = malloc(SHELL_TOKEN_MAX * sizeof(char*));
  memset(lt->t, 0, SHELL_TOKEN_MAX * sizeof(char*));
  int line_last_start = 0;
  int len = strlen(line);
  int in_whitespace = 0;
  for (int i = 0; i < len;) {
    switch (line[i]) {
      case 32:
      case 9:
        in_whitespace = 1;
        line[i++] = '\0';
        break;
      default:
        if (in_whitespace) {
          if (strlen(line + line_last_start))
            lt->t[lt->l++] = strdup(line + line_last_start);
          line_last_start = i;
          in_whitespace = 0;
        }
        i++;
        if (i >= SHELL_LINE_MAX) {
          printf("error: line characters surpass SHELL_LINE_MAX\n");
          exit(-1);
        }
        break;
    }
    if (lt->l > SHELL_TOKEN_MAX) {
      printf("error: line tokens surpass SHELL_TOKEN_MAX\n");
      exit(-1);
    }
  }
  if (line_last_start < len) {
    lt->t[lt->l++] = strdup(&line[line_last_start]);
  }
  return lt;
}

line_token_t *get_term_line(void) {
  char line[SHELL_LINE_MAX];
  memset(line, 0, SHELL_LINE_MAX);
  line_token_t *lt = NULL;
  for (int c, i = 0; (c = getchar());) {
    switch (c) {
      case 4:
        exit(0);
        break;
      case 9:
        break;
      case 127: // <BS>
        if (i > 0) {
          line[--i] = '\0';
          printf("\x1b[1D \x1b[1D");
        }
        break;
      case 2: // ^B
        break;
      case 6: // ^F
        break;
      case 12: // ^L
        printf("\x1b[H\x1b[2J");
        prompt();
        printf("%s", line);
        break;
      case 13:
      case '\n':
        write(STDOUT_FILENO, "\n", 1);
        lt = split_line_tokens(strdup(line));
        return lt;
      default:
        if (c >= 30) {
          line[i++] = c;
          write(STDOUT_FILENO, &c, 1);
          if (i >= SHELL_LINE_MAX) {
            printf("error: line characters surpass SHELL_LINE_MAX\n");
            exit(-1);
          }
        }
        break;
    }
  }
  return lt;
}

int main(void) {
  tbegin();
  setup_signal_handlers();
  while (1) {
    prompt();
    line_token_t *lt = get_term_line();
    if (lt != NULL && lt->l > 0) {
      handle(lt->l, lt->t);
      free_line(lt);
    }
  }
  tend();
  return 0;
}
