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
import java.net.URISyntaxException;
import static org.junit.Assert.assertEquals;
import org.junit.Test;

public class QueryTest {
  @Test
  public void simple() throws URISyntaxException {
    String uri = "http://localhost:7100/object?foo=bar&answer=42";
    Query query = new Query(new URI(uri));
    assertEquals("bar", query.get("foo", "not found"));
    assertEquals("42", query.get("answer", "not found"));
    assertEquals(42, query.getLong("answer", 0));
    assertEquals("not found", query.get("bar", "not found"));
    assertEquals("really not found", query.get("bar", "really not found"));
    assertEquals(0, query.getLong("bar", 0));
    assertEquals(42, query.getLong("bar", 42));
    assertEquals("/object?answer=42&foo=sludge", query.with("foo", "sludge"));
    assertEquals("/object?foo=bar&answer=43", query.with("answer", "43"));
    assertEquals("/object?foo=bar&answer=43", query.with("answer", 43));
    assertEquals("/object?foo=bar&answer=42&bar=finally", query.with("bar", "finally"));
  }

  @Test
  public void multiValue() throws URISyntaxException {
    String uri = "http://localhost:7100/object?foo=bar&answer=42&foo=sludge";
    Query query = new Query(new URI(uri));
    assertEquals("sludge", query.get("foo", "not found"));
    assertEquals(42, query.getLong("answer", 0));
    assertEquals("not found", query.get("bar", "not found"));
    assertEquals("/object?answer=42&foo=tar", query.with("foo", "tar"));
    assertEquals("/object?foo=bar&foo=sludge&answer=43", query.with("answer", "43"));
    assertEquals("/object?foo=bar&answer=42&foo=sludge&bar=finally", query.with("bar", "finally"));
  }

  @Test
  public void empty() throws URISyntaxException {
    String uri = "http://localhost:7100/object";
    Query query = new Query(new URI(uri));
    assertEquals("not found", query.get("foo", "not found"));
    assertEquals(2, query.getLong("foo", 2));
    assertEquals("/object?foo=sludge", query.with("foo", "sludge"));
    assertEquals("/object?answer=43", query.with("answer", "43"));
  }
}
