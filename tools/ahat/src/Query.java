/*
 * Copyright (C) 2015 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.ahat;

import java.net.URI;

/**
 * A class for getting and modifying query parameters.
 */
class Query {
  private URI mUri;

  public Query(URI uri) {
    mUri = uri;
  }

  /**
   * Return the value of a query parameter with the given name.
   * If there is no query parameter with that name, returns the default value.
   * If there are multiple query parameters with that name, the value of the
   * last query parameter is returned.
   * If the parameter is defined with an empty value, "" is returned.
   */
  public String get(String name, String defaultValue) {
    String value = defaultValue;
    String query = mUri.getQuery();
    if (query != null) {
      for (String param : query.split("&")) {
        if (param.startsWith(name + "=")) {
          value = param.substring(name.length()+1);
        }
      }
    }
    return value;
  }

  /**
   * Return the long value of a query parameter with the given name.
   */
  public long getLong(String name, long defaultValue) {
    String value = get(name, null);
    return value == null ? defaultValue : Long.parseLong(value);
  }

  /**
   * Return a uri string suitable for an href target that links to the current
   * page, except with the named query parameter set to the new value.
   */
  public String with(String name, String value) {
    String query = mUri.getQuery();
    StringBuilder newQuery = new StringBuilder();
    newQuery.append(mUri.getRawPath());
    newQuery.append('?');
    if (query != null) {
      for (String param : query.split("&")) {
        if (!param.equals(name) && !param.startsWith(name + "=")) {
          newQuery.append(param);
          newQuery.append('&');
        }
      }
    }
    newQuery.append(name);
    newQuery.append('=');
    newQuery.append(value);
    return newQuery.toString();
  }

  /**
   * Return a uri string suitable for an href target that links to the current
   * page, except with the named query parameter set to the new long value.
   */
  public String with(String name, long value) {
    return with(name, String.valueOf(value));
  }
}
