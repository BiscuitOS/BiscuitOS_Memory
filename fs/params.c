/* Helpers for initial module or kernel cmdline parsing
   Copyright (C) 2001 Rusty Russell.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <linux/kernel.h>
#include <linux/errno.h>
#include "biscuitos/moduleparam.h"

#if 0
#define DEBUGP printk
#else
#define DEBUGP(fmt, a...)
#endif

static inline int dash2underscore_bs(char c)
{
	if (c == '-')
		return '_';
	return c;
}

static inline int parameq_bs(const char *input, const char *paramname)
{
	unsigned int i;

	for (i = 0; dash2underscore_bs(input[i]) == paramname[i]; i++)
		if (input[i] == '\0')
			return 1;
	return 0;
}

static int parse_one_bs(char *param,
			char *val,
			struct kernel_param_bs *params,
			unsigned num_params,
			int (*handle_unknown)(char *param, char *val))
{
	unsigned int i;

	/* Find parameter */
	for (i = 0; i < num_params; i++) {
		if (parameq_bs(param, params[i].name)) {
			DEBUGP("They are equal!  Calling %p\n",
					params[i].set);
			return params[i].set(val, &params[i]);
		}
	}

	if (handle_unknown) {
		DEBUGP("Unknown argument: calling %p\n", handle_unknown);
		return handle_unknown(param, val);
	}

	DEBUGP("Unknown argument `%s'\n", param);
	return -ENOENT;
}

/* You can use " around spaces, but can't escape ". */
/* Hyphens and underscores equivalent in parameter names. */
static char *next_arg_bs(char *args, char **param, char **val)
{
	unsigned int i, equals = 0;
	int in_quote = 0, quoted = 0;
	char *next;

	/* Chew any extra spaces */
	while (*args == ' ') args++;
	if (*args == '"') {
		args++;
		in_quote = 1;
		quoted = 1;
	}

	for (i = 0; args[i]; i++) {
		if (args[i] == ' ' && !in_quote)
			break;
		if (equals == 0) {
			if (args[i] == '=')
				equals = i;
		}
		if (args[i] == '"')
			in_quote = !in_quote;
	}

	*param = args;
	if (!equals)
		*val = NULL;
	else {
		args[equals] = '\0';
		*val = args + equals + 1;

		/* Don't include quotes in value. */
		if (**val == '"') {
			(*val)++;
			if (args[i-1] == '"')
				args[i-1] = '\0';
		}
		if (quoted && args[i-1] == '"')
			args[i-1] = '\0';
	}

	if (args[i]) {
		args[i] = '\0';
		next = args + i + 1;
	} else
		next = args + i;
	return next;
}

int parse_args_bs(const char *name,
		char *args,
		struct kernel_param_bs *params,
		unsigned num,
		int (*unknown)(char *param, char *val))
{
	char *param, *val;

	DEBUGP("Parsing ARGS: %s\n", args);

	while (*args) {
		int ret;

		args = next_arg_bs(args, &param, &val);
		ret = parse_one_bs(param, val, params, num, unknown);
		switch (ret) {
		case -ENOENT:
			printk(KERN_ERR "%s: Unknown parameter `%s'\n",
					name, param);
			return ret;
		case -ENOSPC:
			printk(KERN_ERR
				"%s: `%s' too large for parameter `%s'\n",
				name, val ?: "", param);
			return ret;
		case 0:
			break;
		default:
			printk(KERN_ERR
				"%s: `%s' invalid for parameter `%s'\n",
				name, val ?: "", param);
			return ret;
		}
	}

	/* All parsed OK. */
	return 0;
}
