/* This file is part of 3hs
 * Copyright (C) 2021-2025 hShop developer team
 *
 * This program is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef inc_error_hh
#define inc_error_hh

#include <3ds.h>

#define APPERR_NOSUPPORT MAKERESULT(RL_PERMANENT, RS_NOTSUPPORTED, RM_APPLICATION, 0)
#define APPERR_CANCELLED MAKERESULT(RL_TEMPORARY, RS_CANCELED, RM_APPLICATION, 1)
#define APPERR_NOSPACE MAKERESULT(RL_TEMPORARY, RS_OUTOFRESOURCE, RM_APPLICATION, 2)
#define APPERR_NOREINSTALL MAKERESULT(RL_FATAL, RS_INVALIDSTATE, RM_APPLICATION, 3)
#define APPERR_TITLE_MISMATCH MAKERESULT(RL_TEMPORARY, RS_OUTOFRESOURCE, RM_APPLICATION, 4)
#define APPERR_NORANGE MAKERESULT(RL_PERMANENT, RS_NOTSUPPORTED, RM_APPLICATION, 5)
#define APPERR_NOSIZE MAKERESULT(RL_PERMANENT, RS_NOTSUPPORTED, RM_APPLICATION, 6)
#define APPERR_JSON_FAIL MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, 7)
#define APPERR_NON200 MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, 8)
#define APPERR_API_FAIL MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, 9)
#define APPERR_TOO_LARGE MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, 10)
#define APPERR_FILEFWD_FAIL MAKERESULT(RL_PERMANENT, RS_INTERNAL, RM_APPLICATION, 11)
#define APPERR_TITLE_UNLISTED MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, 12)
#define APPERR_OUT_OF_MEM MAKERESULT(RL_TEMPORARY, RS_OUTOFRESOURCE, RM_APPLICATION, 13)
#define APPERR_INCOMPATIBLE_FONT MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, 14)
#define APPERR_MAX_REDIRECTS MAKERESULT(RL_PERMANENT, RS_OUTOFRESOURCE, RM_APPLICATION, 15)
#define APPERR_INVALID_NB MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, 16)
#define APPERR_CRITICAL_BAT MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, 17)
#define APPERR_FAILED_CURL MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, 18)
#define APPERR_DSPFIRM_IO_FAIL MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, 19)
#define APPERR_DSPFIRM_INVALID MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, 20)
#define APPERR_INVALID_VERSION_STRING MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, 21)
#define APPERR_NO_BASE MAKERESULT(RL_PERMANENT, RS_INVALIDSTATE, RM_APPLICATION, 22)
#define APPERR_DIRECT_SOCKET_SETUP MAKERESULT(RL_TEMPORARY, RS_INVALIDSTATE, RM_APPLICATION, 23)
#define APPERR_DIRECT_SOCKET_TRANSFER MAKERESULT(RL_TEMPORARY, RS_INVALIDSTATE, RM_APPLICATION, 24)

#ifdef __cplusplus
#include <string>

typedef struct error_container
{

	std::string sDesc;
	Result      iDesc;

	std::string sLvl;
	Result      iLvl;

	std::string sSum;
	Result      iSum;

	std::string sMod;
	Result      iMod;

	Result full;
} error_container;


void report_error(error_container& container, std::string note = "");
std::string format_err(const std::string& msg, Result code);
error_container get_error(Result res);
std::string pad8code(Result code);

/** Presents an error code to the user
 */
//void edisp(Result code);
#endif

#endif
