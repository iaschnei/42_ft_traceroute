#include "ft_traceroute.h"

double	time_diff_ms(struct timeval start, struct timeval end)
{
	return ((end.tv_sec - start.tv_sec) * 1000.0 +
			(end.tv_usec - start.tv_usec) / 1000.0);
}

void	resolve_target(t_traceroute *t, const char *target)
{
	struct addrinfo hints, *res;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;	// IPv4 only
	hints.ai_socktype = SOCK_DGRAM;

	if (getaddrinfo(target, NULL, &hints, &res) != 0)
	{
		printf("Error resolving IP address, make sure the format is correct");
		exit(1);
	}
	t->dest_addr = *(struct sockaddr_in *)res->ai_addr;
	t->target = strdup(target);
	freeaddrinfo(res);
}