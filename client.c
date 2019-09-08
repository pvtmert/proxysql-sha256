
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include <mysql/mysql.h>

#define MAX(x, y) ( ((x) > (y)) ? (x) : (y) )

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
	if(err) {
		print(stderr, m, "[%6d]: %s", err, mysql_error(m));
	}
	return err;
}

void tableprint(MYSQL_RES *result, long rows) {
	if(!result) {
		return;
	}
	for(long i=0, n=mysql_num_fields(result); i<mysql_num_rows(result); i++) {
		printf("row: ^");
		MYSQL_ROW row = mysql_fetch_row(result);
		unsigned long *l=mysql_fetch_lengths(result);
		for(int j=0; j<n; j++) {
			printf("[%*s]", -MAX(80UL/n, l[j]), row[j]?row[j]:"NULL");
			continue;
		}
		printf("$\n");
		continue;
	}
	if(result) mysql_free_result(result);
	return;
}

int
tester(const char *host, unsigned short port, const char *db, const char *user, const char *pass, const char **commands) {
	MYSQL *con = mysql_init(NULL);
	mysql_options(con, MYSQL_INIT_COMMAND, (void *)"show processlist;");
	mysql_real_connect(con, host, user, pass, db, port, NULL, 0);
	if(print(stdout, con, "isrv: %s", mysql_get_server_info(con))) goto exithandler;
	if(print(stdout, con, "prot: %d", mysql_get_proto_info(con)))  goto exithandler;
	if(print(stdout, con, "host: %s", mysql_get_host_info(con)))   goto exithandler;
	if(print(stdout, con, "icli: %s", mysql_get_client_info()))    goto exithandler;
	if(print(stdout, con, "stat: %s", mysql_stat(con)))            goto exithandler;
	if(print(stdout, con, "info: %s", mysql_info(con)))            goto exithandler;
	tableprint(mysql_list_processes(con), 0);
	tableprint(mysql_list_dbs(con, NULL), 0);
	tableprint(mysql_list_tables(con, NULL), 0);
	while(commands && *commands) {
		print(stdout, con, "exec: %s", *commands);
		if(print(stdout, con, "query: %d", mysql_query(con, *commands)))
			goto exithandler;
		tableprint(mysql_store_result(con), mysql_affected_rows(con));
		commands += 1;
	}
	exithandler: {
		int err = error(con);
		mysql_close(con);
		return err;
	}
	return 0;
}

int
main(const int argc, const char **argv) {
	if(argc < 5) {
		print(
			stdout,
			NULL,
			"usage: %s <host> <port> <schema> <user> [pass [sql...]]",
			argv[0]
		);
		return 1;
	}
	const char  *pass = (argc > 5 && strlen(argv[5])) ? argv[5] : NULL;
	const char **cmds = (argc > 6) ? &(argv[6]) : NULL;
	return tester(argv[1], atoi(argv[2]), argv[3], argv[4], pass, cmds);
}
