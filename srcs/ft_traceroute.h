#ifndef FT_TRACEROUTE_H
# define FT_TRACEROUTE_H

# include <stdio.h>
# include <stdlib.h>
# include <string.h>
# include <unistd.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/time.h>
# include <netinet/in.h>
# include <netinet/ip_icmp.h>
# include <netdb.h>
# include <arpa/inet.h>
# include <errno.h>

# define MAX_TTL 30
# define PROBES 3
# define DEST_PORT 33434
# define TIMEOUT_SEC 2

#define HOSTNAME_LEN 256

typedef struct s_probe_result
{
	char ip[INET_ADDRSTRLEN];
	char hostname[HOSTNAME_LEN];
	double rtt;
	int received;
}	t_probe_result;


typedef struct s_traceroute {
	char				*target;
	struct sockaddr_in	dest_addr;
	int					sock_send;
	int					sock_recv;
} t_traceroute;


void	print_help(void);
void run_traceroute(t_traceroute *t, int max_ttl, int probes, int resolve_dns, int base_port);
void	resolve_target(t_traceroute *t, const char *target);
double	time_diff_ms(struct timeval start, struct timeval end);

#endif
