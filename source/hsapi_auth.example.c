/*
 * Local build placeholder.
 *
 * Nocturne uses the client-auth implementation distributed in the official
 * public 3hs source archive. Builders should provide that upstream
 * source/hsapi_auth.c file locally; personal hShop credentials are not
 * expected or required.
 *
 * source/hsapi_auth.c is ignored by Git and should not be committed here.
 */
#include <string.h>

const char *hsapi_user = "<api-user>";
const int hsapi_password_length = sizeof("<api-password>") - 1;

void hsapi_password(char *ret)
{
	memcpy(ret, "<api-password>", hsapi_password_length);
}
