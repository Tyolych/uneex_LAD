#define NCURSES_WIDECHAR 1
#include <locale.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <wchar.h>
#include <sys/stat.h>
#include <limits.h>

// Each string has len=strlen(str), and str is a pointer to some char* in file_data
typedef struct LenStr_s {
	wchar_t *str;
	int len;
} LenStr_t;

//	Basic structure: read whole file in memory, counting strings and making table of strings
typedef struct WndFile_s {
	off_t file_size;
	size_t row_offset;
	size_t line_current;
	size_t lines_total;
	size_t path_size; // strlen(path)
	WINDOW *win;
	wchar_t *file_data; // raw file date, where \n changed to \000
	LenStr_t *lines;
	char path[PATH_MAX];
} WndFile_t;

// Upper line for info, so height must be decremented
const int CONST_HEIGHT_DECREMENT = 1;

// in ncurses.h all keys defined, so we use define too
#ifndef KEY_ESC
#define KEY_ESC 27
#endif

void wnd_file_free(WndFile_t **ptr_to_wnd_file);
WndFile_t *wnd_file_make(const char *path_to_file);
bool wnd_file_is_invalid(const WndFile_t *wf);
void wnd_file_draw(const WndFile_t *wf);
bool wnd_file_page_up(WndFile_t *wf);
bool wnd_file_page_down(WndFile_t *wf);
bool wnd_file_line_up(WndFile_t *wf);
bool wnd_file_to_begin(WndFile_t *wf);
bool wnd_file_line_down(WndFile_t *wf);
bool wnd_file_to_end(WndFile_t *wf);
bool wnd_file_row_left(WndFile_t *wf);
bool wnd_file_row_right(WndFile_t *wf);

#define LLG()                                                   \
	{                                                           \
		fprintf(stderr, "\r%s : %d\n", __FUNCTION__, __LINE__); \
		fflush(stdout);                                         \
	}
/*

*/
int main(int argc, const char *argv[])
{
	const char *file_path = "./test_file.txt";

	setlocale(LC_ALL, "");

	if (argc < 2 || argv[1] == NULL) {
		fprintf(stderr, "\rmissed file to show: defaulting to %s\n", file_path);
	} else {
		file_path = argv[1];
	}

	if (initscr() == NULL) {
		fprintf(stderr, "\rinitscr() error %d\n", errno);
		return (EXIT_FAILURE);
	}
	start_color();
	noecho();
	cbreak();

	init_pair(1, COLOR_WHITE, COLOR_BLUE);
	init_pair(2, COLOR_CYAN, COLOR_BLACK);

	WndFile_t *wf;

	if ((wf = wnd_file_make(file_path)) == NULL) {
		endwin();
		return (EXIT_FAILURE);
	}

	wnd_file_draw(wf);

	bool fBreakLoop = false;

	while (!fBreakLoop) {
		int key = wgetch(wf->win);
		bool fUpdate = false;

		switch (key) {
		case 'q':
		case KEY_ESC:
			fBreakLoop = true;
			break;

		case KEY_NPAGE:
		case ' ':
			fUpdate = wnd_file_page_down(wf);
			break;

		case KEY_PPAGE:
			fUpdate = wnd_file_page_up(wf);
			break;

		case KEY_UP:
			fUpdate = wnd_file_line_up(wf);
			break;

		case KEY_RIGHT:
			fUpdate = wnd_file_row_right(wf);
			break;

		case KEY_LEFT:
			fUpdate = wnd_file_row_left(wf);
			break;

		case KEY_DOWN:
			fUpdate = wnd_file_line_down(wf);
			break;

		case KEY_HOME:
			fUpdate = wnd_file_to_begin(wf);
			break;

		case KEY_END:
			fUpdate = wnd_file_to_end(wf);
			break;
		}

		if (fUpdate) {
			wnd_file_draw(wf);
		}
	}

	wnd_file_free(&wf);
	endwin();

	return (EXIT_SUCCESS);
}

/*

*/
void wnd_file_free(WndFile_t **ptr_to_wnd_file)
{
	WndFile_t *wf;
	if (ptr_to_wnd_file == NULL || ((wf = *ptr_to_wnd_file) == NULL))
		return;

	if (wf->lines != NULL) {
		free(wf->lines);
	}

	if (wf->file_data != NULL) {
		free(wf->file_data);
	}

	free(wf);

	*ptr_to_wnd_file = NULL;
}

/*

*/
WndFile_t *wnd_file_make(const char *path_to_file)
{
	if (path_to_file == NULL || *path_to_file == 0) {
		fprintf(stderr, "\r%s: invalid params\n", __FUNCTION__);
		return NULL;
	}

	off_t i = sizeof(WndFile_t);
	WndFile_t *wf = (WndFile_t *)malloc(i);

	if (wf == NULL) {
		fprintf(stderr, "\r%s: malloc(%ld) error %d\n", __FUNCTION__, i, errno);
		return NULL;
	}

	memset(wf, 0, i);

	strncpy(wf->path, path_to_file, PATH_MAX - 1);

	wf->path[sizeof(wf->path) - 1] = 0;

	wf->path_size = strlen(wf->path);

	struct stat stinfo;
	if (stat(wf->path, &stinfo) == -1) {
		free(wf);

		fprintf(stderr, "\r%s: stat(%s) error %d\n", __FUNCTION__, path_to_file,
				errno);

		return NULL;
	}

	wf->file_size = stinfo.st_size;

	i = (wf->file_size + 2) * sizeof(wchar_t);
	wf->file_data = (wchar_t *)malloc(i);

	if (wf->file_data == NULL) {
		free(wf);
		fprintf(stderr, "\r%s: malloc(%ld) error %d\n", __FUNCTION__, i, errno);

		return NULL;
	}

	memset(wf->file_data, 0, i);

	wchar_t wc;
	wchar_t *wp = wf->file_data;

	FILE *fp = fopen(wf->path, "rt");

	if (fp == NULL) {
		fprintf(stderr, "\r%s: fopen(%s) error %d\n", __FUNCTION__,
				path_to_file, errno);

		goto BadBranch;
	}

	if (wf->file_size) {
		wf->lines_total++;

		while (true) {
			if ((wc = fgetwc(fp)) == (wchar_t)WEOF)
				break;

			if ((*wp++ = wc) == L'\n') {
				wf->lines_total++;
			}
		}
	}

	fclose(fp);

	fp = NULL;

	wp = wf->file_data;

	// with reserve for detecting null
	i = (wf->lines_total + 2);
	i *= sizeof(LenStr_t);

	wf->lines = (LenStr_t *)malloc(i);

	if (wf->lines == NULL) {
		fprintf(stderr, "\r%s: malloc(%ld) error %d\n", __FUNCTION__, i, errno);
		goto BadBranch;
	}

	memset(wf->lines, 0, i);

	i = 0;
	wf->lines[i].str = wf->file_data;

	wp = (wchar_t *)wf->file_data;
	for (off_t n = 0; n < wf->file_size; ++n, ++wp) {
		if (*wp == L'\n') {
			*wp = 0;
			wf->lines[i].len = wcslen(wf->lines[i].str);
			wf->lines[++i].str = &wp[1];
		}
	}

	if (wf->lines[i].str != NULL) {
		wf->lines[i].len = wcslen(wf->lines[i].str);
	}

	wf->win = newwin(LINES, COLS, 0, 0);

	if (wf->win == NULL) {
		goto BadBranch;
	}

	keypad(wf->win, TRUE);

	scrollok(wf->win, TRUE);

	return wf;

BadBranch:

	if (fp != NULL) {
		fclose(fp);
	}

	if (wf->file_data != NULL) {
		free(wf->file_data);
	}

	if (wf->lines != NULL) {
		free(wf->lines);
	}

	free(wf);

	return NULL;
}

/*

*/
bool wnd_file_is_invalid(const WndFile_t *wf)
{
	if (wf == NULL)
		return true;

	if (wf->file_data == NULL)
		return true;

	if (wf->lines == NULL)
		return true;

	if (wf->win == NULL)
		return true;

	return false;
}

/*

*/
void wnd_file_draw(const WndFile_t *wf)
{
	int width, height, i;
	char buf[PATH_MAX + 1024];
	size_t idx;
	(void)width;

	if (wnd_file_is_invalid(wf))
		return;

	werase(wf->win);

	getmaxyx(wf->win, height, width);

	wmove(wf->win, 0, 0);

	memset(buf, '-', width);
	buf[width] = 0;
	wattron(wf->win, COLOR_PAIR(1));
	wprintw(wf->win, buf);

	wmove(wf->win, 0, 1);

	mvwaddstr(wf->win, 0, 1, wf->path);

	i = snprintf(buf, sizeof(buf) - 1, "Line:%lu/%lu", wf->line_current,
				 wf->lines_total);

	mvwaddstr(wf->win, 0, width - i - 1, buf);

	i += snprintf(buf, sizeof(buf) - 1, "Row:%lu", wf->row_offset) + 1;

	mvwaddstr(wf->win, 0, width - i - 1, buf);

	i += snprintf(buf, sizeof(buf) - 1, "Size:%ld", wf->file_size) + 1;

	mvwaddstr(wf->win, 0, width - i - 1, buf);

	wattroff(wf->win, COLOR_PAIR(1));

	if (height < 1) {
		wrefresh(wf->win);
		return;
	}

	height -= CONST_HEIGHT_DECREMENT; // file info on 0 line

	wmove(wf->win, 1, 1);

	idx = wf->line_current;

	width -= 6;
	if (idx < wf->lines_total) {
		LenStr_t *pTbl = &wf->lines[idx];

		for (int n = 0; n < height; ++n, ++idx, ++pTbl) {
			if (idx >= wf->lines_total || pTbl->str == NULL)
				break;

			wattron(wf->win, COLOR_PAIR(2));
			mvwprintw(wf->win, 1 + n, 0, "%4lu", idx + 1);
			wattroff(wf->win, COLOR_PAIR(2));
			if (wf->row_offset == 0)						
			{
				mvwaddnwstr(wf->win, 1 + n, 5, pTbl->str, pTbl->len < width ? pTbl->len : width);			
			}
			else {
				int len = pTbl->len - wf->row_offset;
				if (len > 0 ) {
					mvwaddnwstr(wf->win, 1 + n, 5, &pTbl->str[wf->row_offset], len < width ? len : width);			
				}
			}
			
		}
	}

	wrefresh(wf->win);
}

/*

*/
bool wnd_file_page_up(WndFile_t *wf)
{
	int width, height;
	(void)width;

	if (wnd_file_is_invalid(wf))
		return false;

	if (wf->line_current == 0)
		return false;

	getmaxyx(wf->win, height, width);

	height -= CONST_HEIGHT_DECREMENT; // file info on 0 line

	if (wf->line_current < (size_t)height)
		wf->line_current = 0;
	else
		wf->line_current -= height;

	return true;
}

/*

*/
bool wnd_file_page_down(WndFile_t *wf)
{
	int width, height;
	(void)width;

	if (wnd_file_is_invalid(wf))
		return false;

	if (wf->line_current >= wf->lines_total)
		return false;

	getmaxyx(wf->win, height, width);

	height -= CONST_HEIGHT_DECREMENT; // file info on 0 line

	wf->line_current += height;
	if (wf->line_current > wf->lines_total)
		wf->line_current = wf->lines_total;

	return true;
}

/*

*/
bool wnd_file_line_up(WndFile_t *wf)
{
	if (wnd_file_is_invalid(wf))
		return false;

	if (wf->line_current == 0)
		return false;

	wf->line_current--;

	return true;
}

/*

*/
bool wnd_file_to_begin(WndFile_t *wf)
{
	if (wnd_file_is_invalid(wf))
		return false;

	if (wf->line_current == 0)
		return false;

	wf->line_current = 0;
	wf->row_offset = 0;
	
	return true;
}

/*

*/
bool wnd_file_line_down(WndFile_t *wf)
{
	if (wnd_file_is_invalid(wf))
		return false;

	if (wf->line_current == wf->lines_total)
		return false;

	if (wf->line_current < wf->lines_total)
		wf->line_current++;

	return true;
}

/*

*/
bool wnd_file_to_end(WndFile_t *wf)
{
	if (wnd_file_is_invalid(wf))
		return false;

	if (wf->line_current == wf->lines_total )
		return false;

	wf->line_current = wf->lines_total;
	wf->row_offset = 0;
	
	return true;
}

/*

*/
bool wnd_file_row_left(WndFile_t *wf)
{
	if (wnd_file_is_invalid(wf))
		return false;

	if (wf->row_offset == 0)
		return false;

	wf->row_offset--;

	return true;
}

/*

*/
bool wnd_file_row_right(WndFile_t *wf)
{
	if (wnd_file_is_invalid(wf))
		return false;

	wf->row_offset++;

	return true;
}
