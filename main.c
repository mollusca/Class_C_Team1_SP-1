#include <stdlib.h>     
#include <unistd.h>      
#include <ncurses.h>   
#include <time.h>      
#include <string.h>  
#include "sudoku.h"

/* DEFINES */
          
#define GRID_LINES 19
#define GRID_COLS 37
#define GRID_Y 3
#define GRID_X 3
#define INFO_LINES 19
#define INFO_COLS 25
#define INFO_Y 3
#define INFO_X GRID_X + GRID_COLS + 5
#define GRID_NUMBER_START_Y 1
#define GRID_NUMBER_START_X 2
#define GRID_LINE_DELTA 4
#define GRID_COL_DELTA 2
#define STATUS_LINES 1
#define STATUS_COLS GRID_COLS + INFO_COLS
#define STATUS_Y 2
#define STATUS_X GRID_X
#define MAX_HINT_TRY 5
#define SUDOKU_LENGTH STREAM_LENGTH - 1
#define INTRO ".WELCOME.............TO..............SUDOKU.............WORLD"

/* GLOBALS */
static bool g_playing = false;
static char* g_provided_stream; /* in case of -s flag the user provides the sudoku stream */
static char plain_board[STREAM_LENGTH];
static char user_board[STREAM_LENGTH];
static DIFFICULTY g_level;
static WINDOW *grid, *infobox, *status;

/* FUNCTIONS */

static bool is_valid_stream(char *s)
{
   char *p = s;
   short n = 0;
   while ((*p) != '\0')
   {
      if (n++ > SUDOKU_LENGTH)
         break;

      //문자가 1에서 9 사이 또는 . 인지 검사 
      if(!((*p >= 49 && *p <= 57) || *p == '.' ))
      {
         printf("Character %c at position %d is not allowed.\n", *p, n);
         return false;
      }
      p++; // 다음 문자
   }

   // stream의 길이가 스도쿠의 길이(81)랑 일치 하는지 
   if (n != SUDOKU_LENGTH )
   {
      printf("Stream has to be %d characters long.\n", SUDOKU_LENGTH);
      return false;
   }

   // 스도쿠 퍼즐 검사
   if (!is_valid_puzzle(s))
   {
      printf("Stream does not represent a valid sudoku puzzle.\n");
      return false;
   }
   // 위의 에러에 모두 해당하지 않으면, 이 stream은 valid 함
   return true;
}

static void cleanup(void)
{
   endwin();
}

// 커서 초기화
static void init_curses(void)
{
    initscr();
    clear();
    atexit(cleanup);
    cbreak();
    noecho();

    if(has_colors())
    {

       start_color();
        init_pair(1, COLOR_BLACK, COLOR_BLACK);
        init_pair(2, COLOR_GREEN, COLOR_BLACK);
        
        init_pair(3, COLOR_WHITE, COLOR_BLACK);
        init_pair(4, COLOR_MAGENTA, COLOR_BLACK);

    }
}

// 스도쿠 화면에 그리기
static void _draw_grid()
{
   // 이 함수에서 grid는 전역변수로 선언된 WINDOW *형 변수

   int i, j;

   for(i = 0;i < 10;i++)
   {
         for(j = 0;j < 10;j++)
         {        
            if(i % 3 == 0)
               wattron(grid, A_BOLD|COLOR_PAIR(2));
            if(j % 3 == 0)
               wattron(grid, A_BOLD|COLOR_PAIR(2));

            wprintw(grid, "+");
            if((j % 3 == 0) && (i % 3 != 0))
            {
                 wattron(grid, A_BOLD|COLOR_PAIR(1));
            }
            if(j < 9)
               wprintw(grid, "---");
            if(i % 3 == 0)
            {
               wattron(grid, A_BOLD|COLOR_PAIR(1));
            }
        } 
      
        for(j = 0;j < 10 && i < 9;j++)
         {
           if(j % 3 == 0)
               wattron(grid, A_BOLD|COLOR_PAIR(2));
            if(j > 0)
               wprintw(grid, "   |");
            else
               wprintw(grid, "|");
            if(j % 3 == 0)
            {
               wattron(grid, A_BOLD|COLOR_PAIR(1));
            }
        }
   }
}

// window 초기화
static void init_windows(void)
{

    keypad(stdscr, true);
    status = newwin(STATUS_LINES, STATUS_COLS, STATUS_Y, STATUS_X);

    // 스도쿠 그리드 창
    grid = newwin(GRID_LINES, GRID_COLS, GRID_Y, GRID_X);
    _draw_grid(); // grid 그리기

    // 인포 박스 
    infobox = newwin(INFO_LINES, INFO_COLS, INFO_Y, INFO_X);
    wbkgd(infobox, COLOR_PAIR(2));

   
    wattron(infobox, A_BOLD|COLOR_PAIR(4));

   wprintw(infobox, "how to play?\n");
 
    wattroff(infobox, A_BOLD|COLOR_PAIR(2));
    wattron(infobox, COLOR_PAIR(3));
   

   
   wprintw(infobox, " S - Start play\n");
    wprintw(infobox, " N - New puzzle\n");
    wprintw(infobox, " x - Delete number\n");
    wprintw(infobox, " H - Give a hint(max 5)\n");
    wprintw(infobox, " A - Giva a answer\n");
    wprintw(infobox, " C - Check ur answer\n");
    wprintw(infobox, " Q - Quit\n\n\n");
      
      wattroff(infobox, COLOR_PAIR(1));
}

// 인자로 81자리 문자열 받아서 그리드 내부 채우기 
static void fill_grid(char *board)
{
   int row, col, x, y;
   int n;
   int c;

   wstandend(grid);
   y = GRID_NUMBER_START_Y;
   for(row=0; row < 9; row++)
   {
      x = GRID_NUMBER_START_X;
      for(col=0; col < 9; col++)
      {
         n = board[row*9+col];
         if(n == '.') // .은 빈칸으로 처리 
            c = ' ';
         else
            c = n; 
         mvwprintw(grid, y, x, "%c", c);
         x += GRID_LINE_DELTA;
      }
      y += GRID_COL_DELTA;
   }
}

// 새 퍼즐 생성 
static void new_puzzle(void)
{
   int holes = get_holes(g_level); // 난이도에 따른 빈칸 수 설정 
   char* stream;

   if (g_provided_stream) // 만일 현재 설정된 스트림이 있으면, 해당 스트림 사용
      stream = g_provided_stream;
   else // 그렇지 않으면 랜덤으로 새 퍼즐 생성
      stream = generate_puzzle(holes);

   //todo
   strcpy(plain_board, stream);
   strcpy(user_board, stream);

   if (!g_provided_stream)
      free(stream);

   // 생성한 스트림으로 그리드 채우기
   fill_grid(plain_board);

   g_playing = true;
}

// 힌트 함수 
int hint_try = 0;

static bool hint()
{
   char tmp_board[STREAM_LENGTH];
   int i, j, solved;

   strcpy(tmp_board, user_board);
   solved = solve(tmp_board); // 문제 풀어서
   
   if (solved != 0)
   {
      while(hint_try < MAX_HINT_TRY)
      {
         // 랜덤 좌표가 . 이면 그 자리 정답으로 채우고 반
         i = rand() % 8 + 1;
         j = rand() % 8 + 1;
         
         if ( user_board[i*9+j] == '.' && hint_try < MAX_HINT_TRY)
         {
            user_board[i*9+j] = tmp_board[i*9+j];
            hint_try++;
            return true;
         }
      } 
      if(hint_try >= MAX_HINT_TRY)
      {
         werase(status);
        mvwprintw(status, 0, 0, "You can not get a hint anymore\n");
      }
   }
   return false;
}

int main(int argc, char *argv[])
{
   bool run = true;
   int key, x, y, posx, posy;

   g_provided_stream = NULL;


   // curse와 window 초기화 
   init_curses();
   init_windows();

   srand(time(NULL)); // 랜덤함수 시드 설정 

   strcpy(plain_board, INTRO);
   strcpy(user_board, INTRO);
   fill_grid(plain_board);
   
   refresh();
   wrefresh(grid);
   wrefresh(infobox);

   y = GRID_NUMBER_START_Y;
   x = GRID_NUMBER_START_X;
   wmove(grid, y, x);
   while(run)
   {

      mvprintw(0, 0, "welcome to sudoku world");
      refresh();
      wrefresh(grid);
      key = getch(); // 입력한 키에 따른 동작 수행 
      werase(status);

      switch(key)
      {
         case KEY_LEFT:
            if(x>5)
               x -= GRID_LINE_DELTA;
            break;
         case KEY_RIGHT:
            if(x<34)
               x += GRID_LINE_DELTA;
            break;  
         case KEY_UP:
            if(y>2)
               y -= GRID_COL_DELTA;
            break;
         case KEY_DOWN:
            if(y<17)
               y += GRID_COL_DELTA;
            break;
          //방향 
         case 'Q':
         case 27: 
            run = false;
            break;

         case 'A': // 문제 풀기 
            if(g_playing)
            {
               werase(status);
               mvwprintw(status, 0, 0, "Solving puzzle...");
               refresh();
               wrefresh(status);
               solve(plain_board);
               fill_grid(plain_board);
               werase(status);
               mvwprintw(status, 0, 0, "You gave up...sorry about that :(");
               g_playing = false;
            }
            break;


       case 'P': // 치트키 
            if(g_playing)
            {
               werase(status);
               mvwprintw(status, 0, 0, "Solving puzzle...");
               refresh();
               wrefresh(status);
               solve(plain_board);
               fill_grid(plain_board);
               werase(status);
               strcpy(user_board,plain_board);
            }
            break;
         case 'S':
         case 'N': // 새 퍼즐 생성
           if (!g_provided_stream)
              wprintw(infobox, "level: %s\n", difficulty_to_str(g_level));

            werase(status);
            mvwprintw(status, 0, 0, "Generating puzzle...");
            refresh();
            wrefresh(status);
            new_puzzle();
            werase(status);
            g_playing = true;

            if (g_provided_stream)
            {
               free(g_provided_stream);
               g_provided_stream = NULL;
            }
            hint_try= 0;
            break;

         case 'C':   
         case 'c':
            if(g_playing)
            {
               int solvable;
               char tmp_board[STREAM_LENGTH];

               werase(status);

               strcpy(tmp_board, user_board);
               solvable= solve(tmp_board);

               if(solvable == 0)
               {
                  mvwprintw(status, 0, 0, "Not correct");
               }
               else
               {
                  if (strchr(user_board, '.') == NULL)
                  {
                     mvwprintw(status, 0, 0, "Solved!");
                     if(g_level == D_EASY)
                     {
                        mvwprintw(status, 0, 0, "NEXT LEVEL");
                        g_level = D_NORMAL;
                     }
                     else if(g_level == D_NORMAL)
                     {
                        mvwprintw(status, 0, 0, "NEXT LEVEL");
                        g_level = D_HARD;
                     }
                     else if(g_level == D_HARD)
                        mvwprintw(status, 0, 0, "You cleared final stage!");
                     

                     g_playing = false;
                  }
                  else
                  {
                     mvwprintw(status, 0, 0, "You didn't even fill it up yet");
                  }
               }
            }
            break;
         // delete
         case KEY_DC:
         case KEY_BACKSPACE:
         case 127:
         case 'x':
            if(g_playing)
            {
               posy = (y-GRID_NUMBER_START_Y)/GRID_COL_DELTA;
               posx = (x-GRID_NUMBER_START_X)/GRID_LINE_DELTA;
               // if on empty position
               if(plain_board[posy*9+posx] == '.')
               {
                  user_board[posy*9+posx] = '.';
                  wprintw(grid, " ");
               }
               break;
            }

         case 'H':
            if (g_playing && hint())
            {
               fill_grid(user_board);
               werase(status);
               mvwprintw(status, 0, 0, "One blank was filled!");
            }
            break;
         default:
            break;
      }

      /*if user inputs a number*/
      // 숫자 화면에 채우기
      if(key >= 49 && key <= 57 && g_playing)
      {
         posy = (y-GRID_NUMBER_START_Y)/GRID_COL_DELTA;
         posx = (x-GRID_NUMBER_START_X)/GRID_LINE_DELTA;
         // if on empty position
         if(plain_board[posy*9+posx] == '.')
         {
            // add inputted number to grid
            wattron(grid, COLOR_PAIR(4));
            wprintw(grid, "%c", key);
            wattroff(grid, COLOR_PAIR(4));
            user_board[posy*9+posx] = key;
         }
      }
      wmove(grid, y,x);
      refresh();
      wrefresh(status);
      wrefresh(grid);
      wrefresh(infobox);
   }

   if (g_provided_stream)
      free(g_provided_stream);

   endwin();
   return EXIT_SUCCESS;
}
