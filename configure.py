from getpass import getpass

w = [
	'#ifndef GHOSTSSH_CRED_H',
	'#define GHOSTSSH_CRED_H',
	'#endif',
	'',
	'const char *GHOSTSSH_USERNAME = "{}";',
	'const char *GHOSTSSH_PASSWORD = "{}";',
]

with open("ghostssh_cred.h", "w") as f:
	username = raw_input("Username: ")
	password = getpass("Password: ")

	w[-2] = w[-2].format(username)
	w[-1] = w[-1].format(password)

	f.write("\n".join(w))
