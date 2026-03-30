#include "ft_traceroute.h"

/*
    Traceroute sends UDP packets to a target address by increasing gradually the Time To Live (TTL)
    Each hop decreases the TTL and when it reaches 0 it stops and replies with a "Time Exceeded"
    We collect all responses and we can then trace the route from the host to the target :

        ./ft_traceroute google.com
        traceroute to google.com (142.250.190.78), 30 hops max
        1  192.168.1.1     1.012 ms  0.975 ms  1.005 ms
        2  10.0.0.1        4.231 ms  4.210 ms  4.198 ms
        3  142.250.190.78  20.455 ms  20.398 ms  20.402 ms

    1st hop is probably a home router
    3rd hop is the final destination


	------------------------------------------------------------------------------
  	Example Usage:

    ./ft_traceroute google.com
      → Traces the route to google.com using default settings (30 hops max, 3 probes)

    ./ft_traceroute -m 20 -q 5 -p 40000 google.com
      → Traces the route to google.com with:
         - Maximum TTL of 20
         - 5 probes per hop
         - Starting destination port 40000

    ./ft_traceroute -n google.com
      → Traces without performing reverse DNS (shows only IPs)

    ./ft_traceroute --help
      → Prints usage and exits

	------------------------------------------------------------------------------

	Bonuses:

    DNS management   Reverse DNS resolution	(resolve hostname from IP)
    -m <max_ttl>     Set the maximum TTL (Time-To-Live) [default: 30]
    -q <probes>      Number of probe packets per hop [default: 3] (sending multiple probes (packets) for a single target helps with accuracy of delay)
    -n               Disable reverse DNS lookup (show only IPs)
    -p <port>        Starting UDP destination port [default: 33434]

	------------------------------------------------------------------------------

*/

int	main(int argc, char **argv)
{
	t_traceroute	t;
	int max_ttl = MAX_TTL;	// Default max TTL (30)
	int probes = PROBES;    // Default number of probes per loop (3)
    int resolve_dns = 1;    // DNS resolution yes/no
    int base_port = 33434;  // Default port
	int i = 1;

	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s [--help] [-m <max_ttl>] [-q <num_probes>] <hostname/IP>\n", argv[0]);
		return (1);
	}

	if (strcmp(argv[1], "--help") == 0) {
        print_help();
        return (0);
    }

	// Parse options
	while (i < argc - 1) // Last arg is target
	{
		if (strcmp(argv[i], "-m") == 0)
		{
			if (++i >= argc - 1) {
				fprintf(stderr, "Missing value for -m\n");
				return (1);
			}
			max_ttl = atoi(argv[i]);
			if (max_ttl <= 0 || max_ttl > 255) {
				fprintf(stderr, "Invalid max TTL: %d (must be between 1 and 64)\n", max_ttl);
				return (1);
			}
        } else if (strcmp(argv[i], "-q") == 0) {
			if (++i >= argc - 1) {
				fprintf(stderr, "Missing value for -q\n");
				return (1);
			}
			probes = atoi(argv[i]);
			if (probes <= 0 || probes > 10) {
				fprintf(stderr, "Invalid number of probes: %d (must be between 1 and 10)\n", probes);
				return (1);
			}
		} else if (strcmp(argv[i], "-n") == 0) {
            resolve_dns = 0;
        } else if (strcmp(argv[i], "-p") == 0) {

            if (++i >= argc - 1) {
                fprintf(stderr, "Missing value for -p\n");
                return (1);
            }
            base_port = atoi(argv[i]);
            if (base_port <= 1024 || base_port > 65535)
            {
                fprintf(stderr, "Invalid port: must be between 1025 and 65535\n");
                return (1);
            }
        } else {
            fprintf(stderr, "Unknown option, run with --help for details.\n");
            return (1);
        }
		i++;
	}

	// Last argument must be target host
    if (resolve_target(&t, argv[argc - 1]) != 0)
        return (1);

    if (run_traceroute(&t, max_ttl, probes, resolve_dns, base_port) != 0)
    {
        free(t.target);
        return (1);
    }

    close(t.sock_send);
    close(t.sock_recv);
    free(t.target);
    return (0);
}

void	print_help(void)
{
	printf("Usage: ./ft_traceroute [--help] [-m <max_ttl>] [-q <num_probes>] <hostname/IP>\n");
	printf(" -m <max_ttl>     Set the maximum TTL (Time-To-Live) [default: 30]\n");
    printf(" -q <probes>      Number of probe packets per hop [default: 3]\n");
    printf(" -n               Disable reverse DNS lookup (show only IPs)\n");
    printf(" -p <port>        Starting UDP destination port [default: 33434]\n");
	return ;
}

void print_result(int ttl, t_probe_result *results, int probes)
{
    int i;
    int printed = 0;

    printf("%2d  ", ttl);

    for (i = 0; i < probes; i++)
    {
        if (results[i].received)
        {
            // Print new hostname only if it differs from the previous probe
            // And only print the hostname + ip if there is a hostname
            if (i == 0 || strcmp(results[i].ip, results[i - 1].ip) != 0) {
                if (strcmp(results[i].hostname, results[i].ip) != 0)
                    printf("%s (%s) ", results[i].hostname, results[i].ip);
                else
                    printf("%s ", results[i].ip);
            }
            printf("%6.3f ms ", results[i].rtt);
            printed = 1;
        }
        else
            printf("  *  ");
    }
    if (!printed)
        printf("  *  ");
    printf("\n");
}

int	run_traceroute(t_traceroute *t, int max_ttl, int probes, int resolve_dns, int base_port)
{
	struct sockaddr_in src_addr;
	socklen_t addrlen;
	char sendbuf[52];
	char recvbuf[512];
	struct timeval start, end;
	fd_set readfds; 
	int ttl, probe;

    memset(sendbuf, 0, sizeof(sendbuf));

	t->sock_send = socket(AF_INET, SOCK_DGRAM, 0);              // Socket for sending
	t->sock_recv = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);     // Socket to receuve ICMP
	if (t->sock_send < 0 || t->sock_recv < 0)
	{
		perror("socket");
		return (1);
	}

    uint32_t dest_ip = t->dest_addr.sin_addr.s_addr;
    int port_counter = base_port;

	printf("traceroute to %s (%s), %d hops max\n", t->target,
		inet_ntoa(t->dest_addr.sin_addr), max_ttl);

    // Loop max_ttl times
	for (ttl = 1; ttl <= max_ttl; ttl++)
	{

		// Set TTL value on socket
		if (setsockopt(t->sock_send, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) < 0)
		{
			perror("setsockopt");
			return (1);
		}

		t_probe_result *results = calloc(probes, sizeof(t_probe_result));
        if (results == NULL) {
            perror("calloc");
            return (1);
        }

        // Loop for each probe
        // If we reach destination, this will be the final loop
		int reached = 0;

        for (probe = 0; probe < probes; probe++)
        {
            gettimeofday(&start, NULL);
            t->dest_addr.sin_port = htons(port_counter++);

            if (sendto(t->sock_send, sendbuf, sizeof(sendbuf), 0,
                (struct sockaddr *)&t->dest_addr, sizeof(t->dest_addr)) < 0) {
                perror("sendto");
            }

            FD_ZERO(&readfds);
            FD_SET(t->sock_recv, &readfds);
            struct timeval timeout = {TIMEOUT_SEC, 0};

            if (select(t->sock_recv + 1, &readfds, NULL, NULL, &timeout) > 0)
            {
                addrlen = sizeof(src_addr);
                int bytes = recvfrom(t->sock_recv, recvbuf, sizeof(recvbuf), 0,
                    (struct sockaddr *)&src_addr, &addrlen);
                if (bytes > 0)
                {

                    if (src_addr.sin_addr.s_addr == dest_ip)
                        reached = 1;

                    gettimeofday(&end, NULL);
                    strcpy(results[probe].ip, inet_ntoa(src_addr.sin_addr));
                    results[probe].rtt = time_diff_ms(start, end);
                    results[probe].received = 1;

                    // Hostname is the IP by default...
                    strcpy(results[probe].hostname, results[probe].ip);

                    // ...unless resolve_dns is specified
                    if (resolve_dns)
                    {
                        char dns_name[HOSTNAME_LEN];
                        if (getnameinfo((struct sockaddr *)&src_addr, addrlen,
                            dns_name, sizeof(dns_name), NULL, 0, NI_NAMEREQD) == 0)
                        {
                            strcpy(results[probe].hostname, dns_name);
                        }
                    }
                }
            }
        }
        print_result(ttl, results, probes);
        free(results);
        if (reached)
            return (0);
	}
    return (0);
}
