#ifndef _BISCUITOS_MODULEPARAM_H
#define _BISCUITOS_MODULEPARAM_H

struct kernel_param_bs;

/* Returns 0, or -errno.  arg is in kp->arg. */
typedef int (*param_set_fn)(const char *val, struct kernel_param_bs *kp);
/* Returns length written or -errno.  Buffer is 4k (ie. be short!) */
typedef int (*param_get_fn)(char *buffer, struct kernel_param_bs *kp);

struct kernel_param_bs {
	const char *name;
	unsigned int perm;
	param_set_fn set;
	param_get_fn get;
	void *arg;
};

extern int parse_args_bs(const char *name, char *args, 
		struct kernel_param_bs *params, unsigned num, 
		int (*unknow)(char *param, char *val));

#endif
