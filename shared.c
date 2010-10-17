#include "shared.h"

/** global variables present in both daemon and module **/
int debug = 0;  /* doesn't actually do anything right now */
int is_module = 1; /* the daemon sets this to 0 immediately */
int pulse_interval = 15;
int use_database = 0;
uint num_nocs = 0, num_peers = 0, num_pollers = 0;
merlin_nodeinfo self;

#ifndef ISSPACE
# define ISSPACE(c) (c == ' ' || c == '\t')
#endif

char *next_word(char *str)
{
	while (!ISSPACE(*str))
		str++;

	while (ISSPACE(*str) || *str == ',')
		str++;

	if (*str)
		return str;

	return NULL;
}

/*
 * converts an arbitrarily long string of data into its
 * hexadecimal representation
 */
char *tohex(const unsigned char *data, int len)
{
	/* number of bufs must be a power of 2 */
	static char bufs[4][41], hex[] = "0123456789abcdef";
	static int bufno;
	char *buf;
	int i;

	buf = bufs[bufno & (ARRAY_SIZE(bufs) - 1)];
	for (i = 0; i < 20 && i < len; i++) {
		unsigned int val = *data++;
		*buf++ = hex[val >> 4];
		*buf++ = hex[val & 0xf];
	}
	*buf = '\0';

	return bufs[bufno++ & (ARRAY_SIZE(bufs) - 1)];
}

#define CB_ENTRY(s) #s
static const char *callback_names[NEBCALLBACK_NUMITEMS] = {
	CB_ENTRY(RESERVED0),
	CB_ENTRY(RESERVED1),
	CB_ENTRY(RESERVED2),
	CB_ENTRY(RESERVED3),
	CB_ENTRY(RESERVED4),
	CB_ENTRY(RAW_DATA),
	CB_ENTRY(NEB_DATA),
	CB_ENTRY(PROCESS_DATA),
	CB_ENTRY(TIMED_EVENT_DATA),
	CB_ENTRY(LOG_DATA),
	CB_ENTRY(SYSTEM_COMMAND_DATA),
	CB_ENTRY(EVENT_HANDLER_DATA),
	CB_ENTRY(NOTIFICATION_DATA),
	CB_ENTRY(SERVICE_CHECK_DATA),
	CB_ENTRY(HOST_CHECK_DATA),
	CB_ENTRY(COMMENT_DATA),
	CB_ENTRY(DOWNTIME_DATA),
	CB_ENTRY(FLAPPING_DATA),
	CB_ENTRY(PROGRAM_STATUS_DATA),
	CB_ENTRY(HOST_STATUS_DATA),
	CB_ENTRY(SERVICE_STATUS_DATA),
	CB_ENTRY(ADAPTIVE_PROGRAM_DATA),
	CB_ENTRY(ADAPTIVE_HOST_DATA),
	CB_ENTRY(ADAPTIVE_SERVICE_DATA),
	CB_ENTRY(EXTERNAL_COMMAND_DATA),
	CB_ENTRY(AGGREGATED_STATUS_DATA),
	CB_ENTRY(RETENTION_DATA),
	CB_ENTRY(CONTACT_NOTIFICATION_DATA),
	CB_ENTRY(CONTACT_NOTIFICATION_METHOD_DATA),
	CB_ENTRY(ACKNOWLEDGEMENT_DATA),
	CB_ENTRY(STATE_CHANGE_DATA),
	CB_ENTRY(CONTACT_STATUS_DATA),
	CB_ENTRY(ADAPTIVE_CONTACT_DATA)
};

const char *callback_name(int id)
{
	if (id < 0 || id > NEBCALLBACK_NUMITEMS - 1)
		return "(invalid/unknown)";

	return callback_names[id];
}

#define CTRL_ENTRY(s) "CTRL_"#s
static const char *control_names[] = {
	CTRL_ENTRY(NOTHING),
	CTRL_ENTRY(PULSE),
	CTRL_ENTRY(INACTIVE),
	CTRL_ENTRY(ACTIVE),
	CTRL_ENTRY(PATHS),
	CTRL_ENTRY(STALL),
	CTRL_ENTRY(RESUME),
	CTRL_ENTRY(STOP),
};
const char *ctrl_name(uint code)
{
	if (code > ARRAY_SIZE(control_names))
		return "(invalid/unknown)";
	if (code == CTRL_GENERIC)
		return "CTRL_GENERIC";
	return control_names[code];
}

#if (defined(__GLIBC__) && (__GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1))
#include <execinfo.h>
void bt_scan(const char *mark, int count)
{
#define TRACE_SIZE 100
	char **strings;
	void *trace[TRACE_SIZE];
	int i, bt_count, have_mark = 0;

	bt_count = backtrace(trace, TRACE_SIZE);
	if (!bt_count)
		return;
	strings = backtrace_symbols(trace, bt_count);
	if (!strings)
		return;

	for (i = 0; i < bt_count; i++) {
		char *paren;

		if (mark && !have_mark) {
			if (strstr(strings[i], mark))
				have_mark = i;
			continue;
		}
		if (mark && count && i >= have_mark + count)
			break;
		paren = strchr(strings[i], '(');
		paren = paren ? paren : strings[i];
		ldebug("%2d: %s", i, paren);
	}
	free(strings);
}
#else
void bt_scan(const char *mark, int count) {}
#endif

static const char *config_key_expires(const char *var)
{
	if (!strcmp(var, "ipc_debug_write"))
		return "2011-05";
	if (!strcmp(var, "ipc_debug_read"))
		return "2011-05";

	return NULL;
}

/* converts huge values to a more humanfriendly byte-representation */
const char *human_bytes(uint64_t n)
{
	const char *suffix = "KMGTP";
	static char tbuf[4][30];
	static int t = 0;
	unsigned int shift = 1;

	t++;
	t &= 0x3;
	if (n < 1024) {
		sprintf(tbuf[t], "%llu bytes", n);
		return tbuf[t];
	}

	while (n >> (shift * 10) > 1024 && shift < sizeof(suffix) - 1)
		shift++;

	sprintf(tbuf[t], "%0.2f %ciB",
			(float)n / (float)(1 << (shift * 10)), suffix[shift - 1]);

	return tbuf[t];
}

linked_item *add_linked_item(linked_item *list, void *item)
{
	struct linked_item *entry = malloc(sizeof(linked_item));

	if (!entry) {
		lerr("Failed to malloc(%zu): %s", sizeof(linked_item), strerror(errno));
		return NULL;
	}

	entry->item = item;
	entry->next_item = list;
	return entry;
}

const char *tv_delta(struct timeval *start, struct timeval *stop)
{
	static char buf[30];
	double secs;
	unsigned int days, hours, mins;

	secs = stop->tv_sec - start->tv_sec;
	days = secs / 86400;
	secs -= days * 86400;
	hours = secs / 3600;
	secs -= hours * 3600;
	mins = secs / 60;
	secs -= mins * 60;

	/* add the micro-seconds */
	secs *= 1000000;
	secs += stop->tv_usec;
	secs -= start->tv_usec;
	secs /= 1000000;

	if (!mins && !hours && !days) {
		sprintf(buf, "%.3lfs", secs);
	} else if (!hours && !days) {
		sprintf(buf, "%um %.3lfs", mins, secs);
	} else if (!days) {
		sprintf(buf, "%uh %um %.3lfs", hours, mins, secs);
	} else {
		sprintf(buf, "%ud %uh %um %.3lfs", days, hours, mins, secs);
	}

	return buf;
}

/*
 * Parse config sync options.
 *
 * This is used for each node and also in the daemon compound
 */
int grok_confsync_compound(struct cfg_comp *comp, merlin_confsync *csync)
{
	unsigned i;

	if (!comp || !csync) {
		return -1;
	}

	/*
	 * first we reset it. An empty compound in the configuration
	 * means "reset the defaults and don't bother syncing this
	 * server automagically"
	 */
	memset(csync, 0, sizeof(*csync));
	for (i = 0; i < comp->vars; i++) {
		struct cfg_var *v = comp->vlist[i];
		if (!strcmp(v->key, "push")) {
			csync->push = strdup(v->value);
			continue;
		}
		if (!strcmp(v->key, "fetch") || !strcmp(v->key, "pull")) {
			csync->fetch = strdup(v->value);
			continue;
		}
		/*
		 * we ignore additional variables here, since the
		 * config sync script may want to add additional
		 * stuff to handle
		 */
	}

	return 0;
}

int grok_common_var(struct cfg_comp *config, struct cfg_var *v)
{
	const char *expires;

	if (!strcmp(v->key, "pulse_interval")) {
		pulse_interval = (unsigned)strtoul(v->value, NULL, 10);
		if (!pulse_interval) {
			cfg_warn(config, v, "Illegal pulse_interval. Using default.");
			pulse_interval = 15;
		}
		return 1;
	}

	expires = config_key_expires(v->key);
	if (expires) {
		cfg_warn(config, v, "'%s' is a deprecated variable, scheduled for "
			 "removal at the first release after %s", v->key, expires);
		/* it's understood, in a way */
		return 1;
	}

	if (!prefixcmp(v->key, "ipc_")) {
		if (!ipc_grok_var(v->key, v->value))
			cfg_error(config, v, "Failed to grok IPC option");

		return 1;
	}

	if (!prefixcmp(v->key, "log_")) {
		if (!log_grok_var(v->key, v->value))
			cfg_error(config, v, "Failed to grok logging option");

		return 1;
	}

	return 0;
}

/*
 * Set some common socket options
 */
int set_socket_options(int sd, int beefup_buffers)
{
	struct timeval sock_timeout = { 0, 5000 };

	/*
	 * make sure random output from import programs and whatnot
	 * doesn't carry over into the net_sock
	 */
	fcntl(sd, F_SETFD, FD_CLOEXEC);

	setsockopt(sd, SOL_SOCKET, SO_RCVTIMEO, &sock_timeout, sizeof(sock_timeout));
	setsockopt(sd, SOL_SOCKET, SO_SNDTIMEO, &sock_timeout, sizeof(sock_timeout));

	if (beefup_buffers) {
		int optval = 128 << 10; /* 128KB */
		setsockopt(sd, SOL_SOCKET, SO_SNDBUF, &optval, sizeof(int));
		setsockopt(sd, SOL_SOCKET, SO_RCVBUF, &optval, sizeof(int));
	}

	return 0;
}

/*
 * Handles all the subtleties regarding CTRL_ACTIVE packets,
 * which also send a sort of compatibility check along with
 * capabilities and attributes about node.
 * node is in this case the source, for which we want to set
 * the proper info structure. Since CTRL_ACTIVE packets are
 * only ever forwarded from daemon to module and from module
 * to 'hood', and never from network to 'hood', we know this
 * packet originated from the module at 'node'.
 *
 * Returns 0 if everything is fine and dandy
 * Returns -1 on general muppet errors
 * Returns < -1 if node is incompatible with us.
 * Returns 1 if node is compatible in word and byte alignment
 *   but has more features than we do.
 * Returns 1 if node is compatible, but info isn't new
 * Returns > 1 if node is compatible but lacks features we
 *   have
 */
int handle_ctrl_active(merlin_node *node, merlin_event *pkt)
{
	merlin_nodeinfo *info;
	uint32_t len;
	int ret = 0; /* assume ok. we'll flip it if needed */
	int version_delta = 0;

	if (!node || !pkt)
		return -1;

	info = (merlin_nodeinfo *)&pkt->body;
	len = pkt->hdr.len;

	/* if body len is smaller than the least amount of
	 * data we will check, we're too incompatible to check
	 * for and report incompatibilities, so just bail out
	 * with an error
	 */
	if (len < sizeof(uint32_t) * 4) {
		lerr("FATAL: %s: incompatible nodeinfo body size %d. Ours is %d",
			 node->name, len, sizeof(node->info));
		lerr("FATAL: No further compatibility comparisons possible");
		return -128;
	}

	/*
	 * Basic check first, so people know what to expect of the
	 * comparisons below, but if byte_order differs, so may this.
	 */
	version_delta = info->version - MERLIN_NODEINFO_VERSION;
	if (version_delta) {
		lwarn("%s: incompatible nodeinfo version %d. Ours is %d",
			  node->name, info->version, MERLIN_NODEINFO_VERSION);
		lwarn("Further compatibility comparisons may be wrong");
	}

	/*
	 * these two checks should never trigger for the daemon
	 * when node is &ipc unless someone hacks merlin to connect
	 * to a remote site instead of over the ipc socket.
	 * It will happen in net-to-net cases where the two systems
	 * have different wordsize (32-bit vs 64-bit) or byte order
	 * (big vs little endian, fe)
	 * If someone wants to jack in conversion functions into
	 * merlin, the right place to activate them would be here,
	 * setting them as in_handler(pkt) for the node in question
	 * (no out_handler() is needed, since the receiving end will
	 * transform the packet itself).
	 */
	if (info->word_size != __WORDSIZE) {
		lerr("FATAL: %s: incompatible wordsize %d. Ours is %d",
			 node->name, info->word_size, __WORDSIZE);
		ret -= 4;
	}
	if (info->byte_order != __BYTE_ORDER) {
		lerr("FATAL: %s: incompatible byte order %d. Ours is %d",
		     node->name, info->byte_order, __BYTE_ORDER);
		ret -= 8;
	}

	/*
	 * this could potentially happen if someone forgets to
	 * restart either Nagios or Merlin after upgrading either
	 * or both to a newer version and the object structure
	 * version has been bumped. It's quite unlikely though,
	 * but since CTRL_ACTIVE packets are so uncommon we can
	 * afford to waste the extra cycles.
	 */
	if (info->object_structure_version != CURRENT_OBJECT_STRUCTURE_VERSION) {
		lerr("FATAL: %s: incompatible object structure version %d. Ours is %d",
			 node->name, info->object_structure_version, CURRENT_OBJECT_STRUCTURE_VERSION);
		ret -= 16;
	}

	/*
	 * If the remote end has a newer info_version we can be reasonably
	 * sure that everything we want from it is present
	 */
	if (!ret) {
		if (version_delta > 0 && len > sizeof(node->info)) {
			/*
			 * version is greater and struct is bigger. Everything we
			 * need is almost certainly present in that struct
			 */
			len = sizeof(node->info);
		} else if (version_delta < 0 && len < sizeof(node->info)) {
			/*
			 * version is less, and struct is smaller. Update this
			 * place with warnings about what won't work when we
			 * add new things to the info struct, and ignore copying
			 * anything right now
			 */
			ret -= 2;
		} else if (version_delta && len != sizeof(node->info)) {
			/*
			 * version is greater and struct is smaller, or
			 * version is lesser and struct is bigger. Either way,
			 * this should never happen
			 */
			lerr("FATAL: %s: impossible info_version / sizeof(nodeinfo_version) combo",
				 node->name);
			lerr("FATAL: %s: %d / %d; We: %d / %d",
				 node->name, len, info->version, sizeof(node->info), MERLIN_NODEINFO_VERSION);
			ret -= 32;
		}
	}

	if (ret < 0) {
		lerr("FATAL: %s; incompatibility code %d. Ignoring CTRL_ACTIVE event",
			 node->name, ret);
		memset(&node->info, 0, sizeof(node->info));
		return ret;
	}


	/* everything seems ok, so handle it properly */


	/* if info isn't new, we return 1 */
	if (!memcmp(&node->info, pkt->body, len)) {
		ldebug("%s re-sent identical CTRL_ACTIVE info", node->name);
		return 1;
	}

	/*
	 * otherwise update the node's info and
	 * print some debug logging.
	 */
	memcpy(&node->info, pkt->body, len);
	ldebug("Received CTRL_ACTIVE from %s", node->name);
	ldebug("   start time: %lu.%lu",
	       info->start.tv_sec, info->start.tv_usec);
	ldebug("  config hash: %s", tohex(info->config_hash, 20));
	ldebug(" config mtime: %lu", info->last_cfg_change);

	return 0;
}
