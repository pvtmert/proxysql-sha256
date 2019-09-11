
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <math.h>

#include <sys/ioctl.h>
#include <mysql.h>

#define MAX(x, y) ( ((x) > (y)) ? (x) : (y) )

#ifdef READLINE
#include <readline/readline.h>
#include <readline/history.h>

//typedef (char*) (rl_ac_generator_f)(const char*, int);

volatile static MYSQL *glob_mysql_conn = NULL;

#define SIZE(x) ( sizeof((x))/sizeof((x)[0]) )

static const char *rl_ac_sql_commands[] = {
	"processlist",
	"databases",
	"describe",
	"database",
	"truncate",
	"tables",
	"schema",
	"having",
	"create",
	"insert",
	"update",
	"delete",
	"select",
	"grant",
	"alter",
	"table",
	"limit",
	"order",
	"union",
	"show",
	"user",
	"from",
	"join",
	"into",
	"like",
	"use",
	"not",
	"by",
	//"\"\"",
	//"''",
	//"{}",
	//"[]",
	//"()",
	//"--",
	//"!=",
	//"!"
	//"=",
	//"*",
	//",",
	//".",
	//"%",
	NULL,
};

void hexdump(void *ptr, size_t size) {
	for(int i=0; i<size; i++) {
		if(!(i%16)) printf("|\n %8x", i);
		if(!(i% 8)) printf("|");
		printf(" %2hhx ", *(char*) (ptr+i));
		continue;
	}
	printf("|\n");
	return;
}

void
rl_ac_print(char **data) {
	printf(">>\n");
	while(data && *data) printf("#> %s\n", *(data++));
	printf("<<\n");
	return;
}

unsigned
rl_ac_populate_size_helper(char **data) {
	/*
	unsigned count = 0;
	while(data && *data) {
		count += 1;
		data  += 1;
		continue;
	}
	return count;
	*/
	char **tmp = data;
	while(tmp && *tmp && **(tmp++));
	return (unsigned) (tmp-data);
}

char**
rl_ac_populate_handle(
	MYSQL *mysql,
	MYSQL_RES *(*function)(MYSQL*, const char*, ...),
	const char *query,
	...
) {
	if(!mysql || !function) return NULL;
	MYSQL_RES *results = function(mysql, query, NULL);
	if(!results) return NULL;
	int nrows = (int) mysql_num_rows(results);
	char **buffer = (char**) calloc(1+nrows, sizeof(char*));
	for(int i=0; i<nrows; i++) {
		MYSQL_ROW row = mysql_fetch_row(results);
		buffer[i] = strdup(row[0]);
		continue;
	}
	mysql_free_result(results);
	buffer[nrows] = NULL;
	return buffer;
}

char**
rl_ac_populate_columns(MYSQL *mysql, ...) {
	if(!mysql) return NULL;
	unsigned size = 0;
	char **buffer = NULL;
	MYSQL_RES *tables = mysql_list_tables(mysql, NULL);
	if(!tables) return NULL;
	const int ntables = (int) mysql_num_rows(tables);
	char *names[ntables];
	for(int i=0; i<ntables; i++) {
		MYSQL_ROW row = mysql_fetch_row(tables);
		names[i] = strdup(row[0]);
		continue;
	}
	mysql_free_result(tables);
	for(int i=0; i<ntables; i++) {
		MYSQL_RES *result = mysql_list_fields(mysql, names[i], NULL);
		MYSQL_FIELD *fields = mysql_fetch_fields(result);
		const unsigned nfields = mysql_num_fields(result);
		char **temp = realloc(buffer, (size + nfields) * sizeof(char*));
		if(temp && *temp) {
			for(int j=0; j<nfields; j++) {
				temp[size+j] = strdup(fields[j].name);
				continue;
			}
			size += nfields;
			buffer = temp;
		}
		mysql_free_result(result);
		continue;
	}
	buffer = realloc(buffer, (1+size) * sizeof(char*));
	buffer[size] = NULL;
	return buffer;
}

char**
rl_ac_populate_all(MYSQL *mysql, const char **base) {
	unsigned size = 0;
	char **buffer = NULL;
	char **results[4] = { base, NULL, NULL, NULL, };
	if(mysql) {
		results[1] = rl_ac_populate_handle(mysql, &mysql_list_dbs,    NULL);
		results[2] = rl_ac_populate_handle(mysql, &mysql_list_tables, NULL);
		results[3] = rl_ac_populate_columns(mysql, NULL);
	}
	for(int i=0; i<SIZE(results); i++) {
		if(results[i]) {
			unsigned count = rl_ac_populate_size_helper(results[i]);
			char **temp = (char**) realloc(buffer, (size + count) * sizeof(char*));
			if(temp && *temp) {
				memset(&(temp[size]), 0, count * sizeof(char*));
				memcpy(&(temp[size]), results[i], count * sizeof(char*));
				size += count;
				buffer = temp;
			}
		}
		continue;
	}
	buffer = realloc(buffer, (1+size) * sizeof(char*));
	buffer[size] = NULL;
	//rl_ac_print(buffer);
	return buffer;
}

void
rl_ac_populate_free(char ***results) {
	for(char **x=*results; x && *x; x++) {
		free(*x);
		continue;
	}
	free(*results);
	free(results);
	return;
}

char*
rl_ac_cycle_fn(const char *txt, int state) {
	static int index, len;
	static char **cmds;
	if(!state) {
		index = 0;
		len = strlen(txt);
		cmds = rl_ac_populate_all(glob_mysql_conn, rl_ac_sql_commands);
	}
	char *str = NULL;
	while((str = cmds[index++])) {
		if(str && *str && !strncmp(str, txt, len)) {
			char *value = strdup(str);
			//rl_ac_populate_free(&cmds);
			return value;
		}
	}
	//rl_ac_populate_free(&cmds);
	return NULL;
}

char**
rl_ac_generator_fn(const char *txt, int state) {
	rl_attempted_completion_over = 1;
	return rl_completion_matches(txt, rl_ac_cycle_fn);
}

#endif

const char*
input(FILE *fp, size_t size, const char *fmt, ...) {
	#ifdef READLINE
	if(stdin == fp) {
		char *line = readline(fmt);
		if(line && *line) add_history(line);
		return line;
	}
	#endif
	if(fmt && *fmt) {
		va_list args;
		va_start(args, fmt);
		vprintf(fmt, args);
		va_end(args);
	}
	char *buffer = (char*) calloc(size, sizeof(char));
	unsigned chars = getline(&buffer, &size, fp);
	if(chars > 0 && *buffer) buffer[chars-1] = 0x00;
	return ((chars > 0) ? realloc(buffer, chars) : (free(buffer), NULL));
}

int
print(FILE *fd, MYSQL *m, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	char *buffer = (char*) calloc(2+strlen(fmt), sizeof(char));
	sprintf(buffer, "%s\n", fmt);
	vprintf(buffer, args);
	va_end(args);
	free(buffer);
	return m ? mysql_errno(m) : 0;
}

int
error(MYSQL *m) {
	int err = mysql_errno(m);
	if(err) print(stderr, m, "[%6d]: %s", err, mysql_error(m));
	return err;
}

unsigned
columnprint(
	int index,
	char lining,
	const char *text,
	unsigned long maxwidth,
	enum enum_field_types type,
	...
) {
	maxwidth = ((!IS_NUM(type) && maxwidth < 6) ? 6 : maxwidth);
	return printf(
		"%s%*s%s",
		(!index && lining) ? "\n " : (lining ? " " : "["),
		(int) ((IS_NUM(type) && !lining) ? maxwidth : -maxwidth), text,
		lining ? "\n" : "]",
		NULL
	);
}

unsigned
columnhelper(
	MYSQL_FIELD *fields,
	MYSQL_ROW row,
	unsigned long ncols,
	char multiline,
	char isheader,
	unsigned long xd,
	unsigned long xi,
	...
) {
	unsigned chars = 0;
	chars += printf(isheader ? "head:%*lu^" : "_row:%*lu^", xd, xi);
	for(int j=0; j<ncols; j++) {
		chars += columnprint(
			j, multiline,
			(isheader) ? fields[j].name : row[j],
			MAX(fields[j].name_length, fields[j].max_length),
			fields[j].type,
			NULL
		);
		continue;
	}
	return chars + printf("$\n");
}

void
tableprint(MYSQL_RES *result, long rows) {
	if(!result) {
		if(rows > 0) printf("> %ld row(s) affected\n", rows);
		return;
	}
	struct winsize termsz;
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &termsz);
	const unsigned long width = (termsz.ws_col ? termsz.ws_col : 80);
	const unsigned long nrows = mysql_num_rows(result);
	const unsigned long digit = floor(log10(abs(nrows?nrows:1)));
	for(long i=-1, ncols=mysql_num_fields(result); i<(long)nrows; i++) {
		MYSQL_FIELD *fields = mysql_fetch_fields(result);
		unsigned long totalwidth = 8+digit;
		for(int j=0; j<ncols; j++)
			totalwidth += MAX(fields[j].name_length, fields[j].max_length);
		printf(
			"",
			columnhelper(
				fields,
				(i < 0) ? NULL : mysql_fetch_row(result), 
				ncols, 
				(totalwidth > (width - 2*ncols)),
				i < 0,
				1+digit,
				1+i,
				NULL
			)
		);
		continue;
	}
	if(result) mysql_free_result(result);
	return;
}

int
tester(
	const char *host,
	unsigned short port,
	const char *db,
	const char *user,
	const char *pass,
	const char **commands,
	...
) {
	FILE *in = NULL;
	MYSQL *con = mysql_init(NULL);
	const char *schema = (db && strlen(db)) ? db : NULL;
	mysql_real_connect(con, host, user, pass, schema, port, NULL, 0);
	if(print(stdout, con, "isrv: %s", mysql_get_server_info(con))) goto quit;
	if(print(stdout, con, "prot: %d", mysql_get_proto_info(con)))  goto quit;
	if(print(stdout, con, "host: %s", mysql_get_host_info(con)))   goto quit;
	if(print(stdout, con, "_ssl: %s", mysql_get_ssl_cipher(con)))  goto quit;
	if(print(stdout, con, "icli: %s", mysql_get_client_info()))    goto quit;
	if(print(stdout, con, "stat: %s", mysql_stat(con)))            goto quit;
	if(print(stdout, con, "info: %s", mysql_info(con)))            goto quit;
	tableprint(mysql_list_dbs(con, NULL), 0);
	if(schema) tableprint(mysql_list_tables(con, NULL), 0);
	while(commands && *commands) {
		if('-' == **commands || '/' == **commands || '.' == **commands) {
			size_t length = 999;
			in = ('-' == **commands) ? stdin : fopen(*commands, "r");
			while(!feof(in)) {
				#ifdef READLINE
				if(in == stdin) glob_mysql_conn = con;
				#endif
				char *buffer = input(in, 999, (in == stdin) ? "sql> " : NULL);
				#ifdef READLINE
				if(!buffer) break;
				#endif
				if(!buffer || !*buffer) continue;
				mysql_query(con, buffer);
				tableprint(
					mysql_store_result(con),
					mysql_affected_rows(con)
				);
				error(con);
				free(buffer);
				continue;
			}
			if('-' == **commands) {
				print(stdout, con, "...done");
			} else {
				fclose(in);
			}
			goto next;
		}
		print(stdout, con, "exec: %s", *commands);
		if(print(stdout, con, "query: %d", mysql_query(con, *commands)))
			goto quit;
		tableprint(mysql_store_result(con), mysql_affected_rows(con));
		next: {
			commands += 1;
			continue;
		}
		goto next;
	}
	quit: {
		if(in == stdin) return (mysql_close(con), 0);
		int err = error(con);
		mysql_close(con);
		return err;
	}
	goto quit;
}

int
main(const int argc, const char **argv) {
	if(argc < 5) {
		print(stdout, NULL,
			"usage:              %s <host> <port> <schema> <user> [<pass|-> [(-|sql|file) ...]]",
			argv[0], NULL);
		print(stdout, NULL, 
			"eg (shell):         %s my.sql.com 3306 test root passwd -", 
			argv[0], NULL);
		print(stdout, NULL, 
			"eg (exec):          %s my.sql.com 3306 test root passwd /path/to/file", 
			argv[0], NULL);
		print(stdout, NULL, 
			"eg (inline):        %s my.sql.com 3306 test root passwd 'select 1;'", 
			argv[0], NULL);
		print(stdout, NULL, 
			"eg (exec/relative): %s my.sql.com 3306 test root passwd ./dump.sql",
			argv[0], NULL);
		return 1;
	}
	const char *pass = (argc > 5 && strlen(argv[5])) ? (
		(1 == strlen(argv[5]) && '-' == *argv[5]) ? input(stdin, 80, "pass:") : argv[5]
	): NULL;
	const char **cmds = (argc > 6) ? &(argv[6]) : NULL;
	#ifdef READLINE
	rl_bind_key('\t', rl_complete);
	rl_attempted_completion_function = rl_ac_generator_fn;
	#endif
	return tester(argv[1], atoi(argv[2]), argv[3], argv[4], pass, cmds);
}
