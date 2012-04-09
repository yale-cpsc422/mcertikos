#ifndef _USER_TYPES_H_
#define _USER_TYPES_H_

typedef signed char		int8_t;
typedef unsigned char		uint8_t;
typedef short			int16_t;
typedef unsigned short		uint16_t;
typedef int			int32_t;
typedef unsigned int		uint32_t;
typedef long long		int64_t;
typedef unsigned long long	uint64_t;

typedef uint32_t		uintptr_t;
typedef int32_t			intptr_t;

typedef uint32_t		size_t;
typedef int32_t			ssize_t;

typedef uint32_t		pid_t;

typedef
struct mgmt_start_t {
	uint32_t	cpu;
	pid_t		pid;
} mgmt_start_t;

typedef
struct mgmt_stop_t {
	uint32_t	cpu;
} mgmt_stop_t;

typedef
struct mgmt_allocpage_t {
	pid_t		pid;
	uintptr_t	va;
} mgmt_allocpage_t;

typedef
struct mgmt_data_t {
	uint32_t	cmd;
	uint8_t		params[100];
} mgmt_data_t;

#define NULL	((void *) 0)

#endif /* !_USER_TYPES_H_ */
