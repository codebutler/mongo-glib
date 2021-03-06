/* mongo-database.c
 *
 * Copyright (C) 2012 Christian Hergert <chris@dronelabs.com>
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

#include <glib/gi18n.h>

#include "mongo-client.h"
#include "mongo-database.h"
#include "mongo-debug.h"
#include "mongo-protocol.h"

G_DEFINE_TYPE(MongoDatabase, mongo_database, G_TYPE_OBJECT)

struct _MongoDatabasePrivate
{
   gchar *name;
   GHashTable *collections;
   MongoClient *client;
};

enum
{
   PROP_0,
   PROP_CLIENT,
   PROP_NAME,
   LAST_PROP
};

static GParamSpec *gParamSpecs[LAST_PROP];

/**
 * mongo_database_get_collection:
 * @database: (in): A #MongoDatabase.
 *
 * Fetches the collection that is found in @database.
 *
 * Returns: (transfer none): A #MongoDatabase.
 */
MongoCollection *
mongo_database_get_collection (MongoDatabase *database,
                               const gchar   *name)
{
   MongoDatabasePrivate *priv;
   MongoCollection *collection;

   ENTRY;

   g_return_val_if_fail(MONGO_IS_DATABASE(database), NULL);
   g_return_val_if_fail(name, NULL);

   priv = database->priv;

   if (!(collection = g_hash_table_lookup(priv->collections, name))) {
      collection = g_object_new(MONGO_TYPE_COLLECTION,
                                "client", priv->client,
                                "database", database,
                                "name", name,
                                NULL);
      g_hash_table_insert(priv->collections, g_strdup(name), collection);
   }

   RETURN(collection);
}

/**
 * mongo_database_get_client:
 * @database: (in): A #MongoDatabase.
 *
 * Fetches the client that @database communicates over.
 *
 * Returns: (transfer none): A #MongoClient.
 */
MongoClient *
mongo_database_get_client (MongoDatabase *database)
{
   g_return_val_if_fail(MONGO_IS_DATABASE(database), NULL);
   return database->priv->client;
}

static void
mongo_database_set_client (MongoDatabase *database,
                           MongoClient   *client)
{
   MongoDatabasePrivate *priv;

   ENTRY;

   g_return_if_fail(MONGO_IS_DATABASE(database));
   g_return_if_fail(MONGO_IS_CLIENT(client));
   g_return_if_fail(!database->priv->client);

   priv = database->priv;

   priv->client = client;
   g_object_add_weak_pointer(G_OBJECT(client),
                             (gpointer *)&priv->client);

   EXIT;
}

const gchar *
mongo_database_get_name (MongoDatabase *database)
{
   g_return_val_if_fail(MONGO_IS_DATABASE(database), NULL);
   return database->priv->name;
}

static void
mongo_database_set_name (MongoDatabase *database,
                         const gchar   *name)
{
   ENTRY;

   g_return_if_fail(MONGO_IS_DATABASE(database));
   g_return_if_fail(!database->priv->name);
   g_return_if_fail(name);

   database->priv->name = g_strdup(name);

   EXIT;
}

static void
mongo_database_drop_cb (GObject      *object,
                        GAsyncResult *result,
                        gpointer      user_data)
{
   GSimpleAsyncResult *simple = user_data;
   MongoClient *client = (MongoClient *)object;
   MongoReply *reply;
   GError *error = NULL;

   ENTRY;

   g_assert(G_IS_SIMPLE_ASYNC_RESULT(simple));

   if (!(reply = mongo_client_command_finish(client, result, &error))) {
      g_simple_async_result_take_error(simple, error);
   }

   g_simple_async_result_set_op_res_gboolean(simple, !!reply);
   g_simple_async_result_complete_in_idle(simple);
   g_object_unref(simple);

   if (reply) {
      mongo_reply_unref(reply);
   }

   EXIT;
}

void
mongo_database_drop_async (MongoDatabase       *database,
                           GCancellable        *cancellable,
                           GAsyncReadyCallback  callback,
                           gpointer             user_data)
{
   MongoDatabasePrivate *priv;
   GSimpleAsyncResult *simple;
   MongoBson *bson;

   ENTRY;

   g_return_if_fail(MONGO_IS_DATABASE(database));
   g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
   g_return_if_fail(callback);

   priv = database->priv;

   if (!priv->client) {
      g_simple_async_report_error_in_idle(G_OBJECT(database),
                                          callback,
                                          user_data,
                                          MONGO_DATABASE_ERROR,
                                          MONGO_DATABASE_ERROR_NO_CLIENT,
                                          _("The client has been lost."));
      EXIT;
   }

   simple = g_simple_async_result_new(G_OBJECT(database), callback, user_data,
                                      mongo_database_drop_async);

   bson = mongo_bson_new_empty();
   mongo_bson_append_int(bson, "dropDatabase", 1);
   mongo_client_command_async(priv->client,
                              priv->name,
                              bson,
                              cancellable,
                              mongo_database_drop_cb,
                              simple);
   mongo_bson_unref(bson);

   EXIT;
}

gboolean
mongo_database_drop_finish (MongoDatabase  *database,
                            GAsyncResult   *result,
                            GError        **error)
{
   GSimpleAsyncResult *simple = (GSimpleAsyncResult *)result;
   gboolean ret;

   g_return_val_if_fail(MONGO_IS_DATABASE(database), FALSE);
   g_return_val_if_fail(G_IS_SIMPLE_ASYNC_RESULT(simple), FALSE);

   if (!(ret = g_simple_async_result_get_op_res_gboolean(simple))) {
      g_simple_async_result_propagate_error(simple, error);
   }

   return ret;
}

static void
mongo_database_finalize (GObject *object)
{
   MongoDatabasePrivate *priv;
   GHashTable *hash;

   ENTRY;

   priv = MONGO_DATABASE(object)->priv;

   g_free(priv->name);

   if (priv->client) {
      g_object_remove_weak_pointer(G_OBJECT(priv->client),
                                   (gpointer *)&priv->client);
      priv->client = NULL;
   }

   if ((hash = priv->collections)) {
      priv->collections = NULL;
      g_hash_table_unref(hash);
   }

   G_OBJECT_CLASS(mongo_database_parent_class)->finalize(object);

   EXIT;
}

static void
mongo_database_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
   MongoDatabase *database = MONGO_DATABASE(object);

   switch (prop_id) {
   case PROP_CLIENT:
      g_value_set_object(value, mongo_database_get_client(database));
      break;
   case PROP_NAME:
      g_value_set_string(value, mongo_database_get_name(database));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
mongo_database_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
   MongoDatabase *database = MONGO_DATABASE(object);

   switch (prop_id) {
   case PROP_CLIENT:
      mongo_database_set_client(database, g_value_get_object(value));
      break;
   case PROP_NAME:
      mongo_database_set_name(database, g_value_get_string(value));
      break;
   default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
   }
}

static void
mongo_database_class_init (MongoDatabaseClass *klass)
{
   GObjectClass *object_class;

   ENTRY;

   object_class = G_OBJECT_CLASS(klass);
   object_class->finalize = mongo_database_finalize;
   object_class->get_property = mongo_database_get_property;
   object_class->set_property = mongo_database_set_property;
   g_type_class_add_private(object_class, sizeof(MongoDatabasePrivate));

   gParamSpecs[PROP_CLIENT] =
      g_param_spec_object("client",
                          _("Client"),
                          _("The client owning this MongoDatabase instance."),
                          MONGO_TYPE_CLIENT,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_CLIENT,
                                   gParamSpecs[PROP_CLIENT]);

   gParamSpecs[PROP_NAME] =
      g_param_spec_string("name",
                          _("Name"),
                          _("Name"),
                          NULL,
                          G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
   g_object_class_install_property(object_class, PROP_NAME,
                                   gParamSpecs[PROP_NAME]);

   EXIT;
}

static void
mongo_database_init (MongoDatabase *database)
{
   ENTRY;

   database->priv =
      G_TYPE_INSTANCE_GET_PRIVATE(database,
                                  MONGO_TYPE_DATABASE,
                                  MongoDatabasePrivate);
   database->priv->collections =
      g_hash_table_new_full(g_str_hash, g_str_equal,
                            g_free, g_object_unref);

   EXIT;
}

GQuark
mongo_database_error_quark (void)
{
   return g_quark_from_static_string("mongo-database-error-quark");
}
