/* mongo-object-id.h
 *
 * Copyright (C) 2011 Christian Hergert <christian@catch.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#if !defined (MONGO_INSIDE) && !defined (MONGO_COMPILATION)
#error "Only <mongo-glib/mongo-glib.h> can be included directly."
#endif

#ifndef MONGO_OBJECT_ID_H
#define MONGO_OBJECT_ID_H

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _MongoObjectId MongoObjectId;

void           mongo_clear_object_id         (MongoObjectId       **object_id);
MongoObjectId *mongo_object_id_new           (void);
MongoObjectId *mongo_object_id_new_from_data (const guint8         *bytes);
MongoObjectId *mongo_object_id_copy          (const MongoObjectId  *object_id);
void           mongo_object_id_free          (MongoObjectId        *object_id);
GType          mongo_object_id_get_type      (void) G_GNUC_CONST;
gchar         *mongo_object_id_to_string     (const MongoObjectId  *object_id);
gint           mongo_object_id_compare       (const MongoObjectId  *object_id,
                                              const MongoObjectId  *other);
gboolean       mongo_object_id_equal         (const MongoObjectId  *object_id,
                                              const MongoObjectId  *other);

G_END_DECLS

#endif /* MONGO_OBJECT_ID_H */
