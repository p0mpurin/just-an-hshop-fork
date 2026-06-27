/*
 * Local build placeholder.
 *
 * Rune3DS uses a local client-auth implementation. Builders should provide
 * source/hsapi_auth.c locally; personal service credentials are not expected
 * or required.
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
