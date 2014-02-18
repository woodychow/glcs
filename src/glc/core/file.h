/**
 * \file glc/core/file.c
 * \brief file io adapted from original work (glc) from Pyry Haulos <pyry.haulos@gmail.com>
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

/**
 * \addtogroup core
 *  \{
 * \defgroup file file io
 *  \{
 */

#ifndef _FILE_H
#define _FILE_H

#include <glc/core/sink.h>
#include <glc/core/source.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief initialize file sink object
 * Writing is done in its own thread.
 * \code
 * // writing example
 * file_sink_init(*file, glc);
 * file->ops->open_target(file, "/tmp/stream.glc");
 * ...
 * file->ops->write_info(file, &info, name, date);
 * file->ops->write_process_start(file, buffer);
 * ...
 * file->ops->write_process_wait(file);
 * file->ops->close_target(file);
 * file->ops->destroy(file);
 * \endcode
 *
 * file->ops->write_info() must be called before starting write
 * process.
 *
 * One stream file can actually hold multiple individual
 * streams: [info0][stream0][info1][stream1]...
 * \param file file object
 * \param glc glc
 * \return 0 on success otherwise an error code
 */
__PUBLIC int file_sink_init(sink_t *sink, glc_t *glc);

/**
 * \brief initialize file sink object
 *
 * Reading stream from file is done in same thread.
 * \code
 * // reading example
 * file_source_init(*file, glc);
 * file->ops->open_source(file, "/tmp/stream.glc");
 * ...
 * file->ops->read_info(file, &info, &name, &date);
 * file->ops->read(file, buffer);
 * file->ops->close_source(file);
 * ...
 * file->ops->destroy(file);
 * free(name);
 * free(date);
 * \endcode
 *
 * Like in writing, file->read_info() must be called before
 * calling file->ops->read().
 *
 * One stream file can actually hold multiple individual
 * streams: [info0][stream0][info1][stream1]...
 * \param file file object
 * \param glc glc
 * \return 0 on success otherwise an error code
 */
__PUBLIC int file_source_init(source_t *source, glc_t *glc);

#ifdef __cplusplus
}
#endif

#endif

/**  \} */
/**  \} */

