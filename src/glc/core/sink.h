/**
 * \file glc/core/sink.h
 * \brief Abstract sink interface
 * \author Olivier Langlois <olivier@trillion01.com>
 * \date 2014

    Copyright 2014 Olivier Langlois

    This file is part of glcs.

    glcs is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    glcs is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with glcs.  If not, see <http://www.gnu.org/licenses/>.

 */

#ifndef _SINK_H
#define _SINK_H

#include <packetstream.h>
#include <glc/common/glc.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct sink_s* sink_t;

typedef struct
{
	/**
	 * \brief set sync mode
	 * \note this must be set before opening sink
	 * \param sink sink object
	 * \param sync 0 = no forced synchronization, 1 = force writing immediately to device
	 * \return 0 on success otherwise an error code
	 */
	int (*set_sync)(sink_t sink, int sync);
	/**
	 * \brief set callback function
	 * Callback is called when callback_request message is encountered
	 * in stream.
	 * \param sink sink object
	 * \param callback callback function address
	 * \return 0 on success otherwise an error code
	 */
	int (*set_callback)(sink_t sink, callback_request_func_t callback);
	/**
	 * \brief open file for writing
	 * \param sink sink object
	 * \param filename target file
	 * \return 0 on success otherwise an error code
	 */
	int (*open_target)(sink_t sink, const char *target_name);
	/**
	 * \brief close target file descriptor
	 * \param sink sink object
	 * \return 0 on success otherwise an error code
	 */
	int (*close_target)(sink_t sink);
	/**
	 * \brief write stream information header to file
	 * \param sink sink object
	 * \param info info structure
	 * \param info_name app name
	 * \param info_date date
	 * \return 0 on success otherwise an error code
	 */
	int (*write_info)(sink_t sink, glc_stream_info_t *info,
			const char *info_name, const char *info_date);
	/**
	 * \brief write EOF message to file
	 * \param sink sink object
	 * \return 0 on success otherwise an error code
	 */
	int (*write_eof)(sink_t sink);
	/**
	 * \brief write current stream state to file
	 * \param sink sink object
	 * \return 0 on success otherwise an error code
	 */
	int (*write_state)(sink_t sink);
	/**
	 * \brief start writing process
	 *
	 * sink will write all data from source buffer to target sink
	 * in a custom format that can be read back using file_read()
	 * \param sink sink object
	 * \param from source buffer
	 * \return 0 on success otherwise an error code
	 */
	int (*write_process_start)(sink_t sink, ps_buffer_t *from);
	/**
	 * \brief block until process has finished
	 * \param sink sink object
	 * \return 0 on success otherwise an error code
	 */
	int (*write_process_wait)(sink_t sink);
	int (*destroy)(sink_t sink);
} sink_ops_t;

struct sink_s
{
	sink_ops_t *ops;
};

#ifdef __cplusplus
}
#endif

#endif

