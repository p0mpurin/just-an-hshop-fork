/*
 * Local build template.
 *
 * Copy this file to source/hsapi_auth.c and replace the placeholders with
 * credentials you are authorized to use. source/hsapi_auth.c is ignored by
 * Git and must never be committed.
 */
#include <string.h>

const char *hsapi_user = "<api-user>";
const int hsapi_password_length = sizeof("<api-password>") - 1;

void hsapi_password(char *ret)
{
	memcpy(ret, "<api-password>", hsapi_password_length);
}

