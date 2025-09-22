#include "ft_traceroute.h"

/*
    Traceroute sends UDP packets to a target address by increasing gradually the Time To Live (TTL)
    Each router decreases the TTL and when it reaches 0 it stops and replies with a "Time Exceeded"
    We can then trace the route from the host to the target :

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
    -q <probes>      Number of probe packets per hop [default: 3]
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

	if (strcmp(argv[1], "--help") == 0)
		print_help();

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
			if (max_ttl <= 0 || max_ttl > 64) {
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
        } else if (strcmp(argv[i], "-p") == 0)
        {
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
        }
		i++;
	}

	// Last argument must be target host
	resolve_target(&t, argv[argc - 1]);
    run_traceroute(&t, max_ttl, probes, resolve_dns, base_port);


	close(t.sock_send);
	close(t.sock_recv);
	free(t.target);
	return (0);
}

void	print_help(void)
{
	printf("Usage: ./ft_traceroute [--help] <hostname/IP>\n");
	printf("Simple implementation of traceroute (IPv4 only)\n");
	exit(0);
}

void	print_result(int ttl, t_probe_result *results, int probes)
{
	int i;
	int printed = 0;

	printf("%2d  ", ttl);

	// Find first received response
	for (i = 0; i < probes; i++)
	{
		if (results[i].received)
		{
			printf("%s (%s)", results[i].hostname, results[i].ip);
			printed = 1;
			break;
		}
	}
	if (!printed)
	{
		printf("*\n");
		return;
	}

	for (i = 0; i < probes; i++)
	{
		if (results[i].received)
			printf("  %6.3f ms", results[i].rtt);
		else
			printf("  *");
	}
	printf("\n");
}

void	run_traceroute(t_traceroute *t, int max_ttl, int probes, int resolve_dns, int base_port)
{
	struct sockaddr_in src_addr;
	socklen_t addrlen;
	char sendbuf[52];
	char recvbuf[512];
	struct timeval start, end;
	fd_set readfds;
	struct icmp *icmp_hdr;
	int ttl, probe;


	t->sock_send = socket(AF_INET, SOCK_DGRAM, 0);              // Socket for sending
	t->sock_recv = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);     // Socket to receuve ICMP
	if (t->sock_send < 0 || t->sock_recv < 0)
	{
		perror("socket");
		exit(1);
	}

	printf("traceroute to %s (%s), %d hops max\n", t->target,
		inet_ntoa(t->dest_addr.sin_addr), max_ttl);

    // Loop X times
	for (ttl = 1; ttl <= max_ttl; ttl++)
	{

		// Set TTL value on socket
		if (setsockopt(t->sock_send, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) < 0)
		{
			perror("setsockopt");
			exit(1);
		}

		t_probe_result *results = calloc(probes, sizeof(t_probe_result));

        t->dest_addr.sin_port = htons(base_port + ttl);

        // Loop for each probe
		for (probe = 0; probe < probes; probe++)
		{

            // Send a UDP packet to destination with current TTL
			gettimeofday(&start, NULL);
		    t->dest_addr.sin_port = htons(base_port + (ttl - 1) * probes + probe);

            if (sendto(t->sock_send, sendbuf, sizeof(sendbuf), 0,
                (struct sockaddr *)&t->dest_addr, sizeof(t->dest_addr)) < 0) {
                perror("sendto");
            }


			FD_ZERO(&readfds);
			FD_SET(t->sock_recv, &readfds);
			struct timeval timeout = {TIMEOUT_SEC, 0};

            // Wait for an ICMP response
			if (select(t->sock_recv + 1, &readfds, NULL, NULL, &timeout) > 0)
			{
                // If we received a response, get it with recvfrom and mesure the round trip time (RTT) for future printing
				addrlen = sizeof(src_addr);
				int bytes = recvfrom(t->sock_recv, recvbuf, sizeof(recvbuf), 0,
					(struct sockaddr *)&src_addr, &addrlen);
				if (bytes > 0)
				{
                    gettimeofday(&end, NULL);
                    strcpy(results[probe].ip, inet_ntoa(src_addr.sin_addr));
                    results[probe].rtt = time_diff_ms(start, end);
                    results[probe].received = 1;

                    // DNS resolution (reverse lookup)
                    if (resolve_dns && getnameinfo((struct sockaddr *)&src_addr, addrlen,
                        results[probe].hostname, HOSTNAME_LEN,
                        NULL, 0, 0) == 0)
                    {}
                    else {
                        // Fallback if no DNS name found
                        strcpy(results[probe].hostname, results[probe].ip);
                    }

					if (src_addr.sin_addr.s_addr == t->dest_addr.sin_addr.s_addr)
					{
						print_result(ttl, results, probes);
						return;
					}
				}
			}
		}
		print_result(ttl, results, probes);
        free(results);
	}
}
