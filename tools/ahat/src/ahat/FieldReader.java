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

package ahat;

import com.android.tools.perflib.heap.*;

class FieldReader {

  static public class FieldReaderException extends Exception {
    public FieldReaderException(String message) {
      super(message);
    }
  }

  // Read the field from the given hprof instance.
  static public Object readField(Object inst, String fieldName)
    throws FieldReaderException {
    if (! (inst instanceof ClassInstance)) {
      throw new FieldReaderException("Not a ClassInstance: " + inst);
    }

    ClassInstance clsinst = (ClassInstance) inst;
    Object val = null;
    int count = 0;
    for (ClassInstance.FieldValue field : clsinst.getValues()) {
      if (fieldName.equals(field.getField().getName())) {
        if (val == null) {
          val = field.getValue();
          count++;
        }
      }
    }

    if (count == 0) {
      throw new FieldReaderException("No field found with name " + fieldName + " in " + inst);
    }

    if (count > 1) {
      throw new FieldReaderException("Multiple fields found with name " + fieldName + " in " + inst);
    } 
    return val;
  }

  static public String readCharArrayAsString(Object inst)
    throws FieldReaderException {
    if (! (inst instanceof ArrayInstance)) {
      throw new FieldReaderException(inst + " is not a character array");
    }

    ArrayInstance array = (ArrayInstance) inst;
    if (array.getArrayType() != Type.CHAR) {
      throw new FieldReaderException(inst + " is not a character array");
    }
    return new String(array.asCharArray(0, array.getValues().length));
  }

  // Verify the given object is a non-null instance of a class with the
  // given name.
  static public void verifyIsClassInstance(Object obj, String className)
    throws FieldReaderException {
    if (! (obj instanceof Instance)) {
      throw new FieldReaderException(obj + " is not a class instance");
    }

    Instance inst = (Instance) obj;
    ClassObj cls = inst.getClassObj();
    if (!className.equals(cls.getClassName())) {
      throw new FieldReaderException("Expected class " + className + ", but got " + cls.getClassName());
    }
  }

  // Read the string value from an hprof java.lang.String Instance.
  static public String readStringValue(Object inst)
    throws FieldReaderException {
    verifyIsClassInstance(inst, "java.lang.String");
    Object value = readField(inst, "value");
    return readCharArrayAsString(value);
  }

  static public String readStringField(Object inst, String fieldName)
    throws FieldReaderException {
    return readStringValue(readField(inst, fieldName));
  }

  static public byte[] readByteArray(Object inst)
    throws FieldReaderException {
    if (! (inst instanceof ArrayInstance)) {
      throw new FieldReaderException(inst + " is not a byte array");
    }

    ArrayInstance array = (ArrayInstance)inst;
    if (array.getArrayType() != Type.BYTE) {
      throw new FieldReaderException(inst + " is not a byte array");
    }

    Object[] objs = array.getValues();
    byte[] bytes = new byte[objs.length];
    for (int i = 0; i < objs.length; i++) {
      Byte b = (Byte)objs[i];
      bytes[i] = b.byteValue();
    }
    return bytes;
  }

  static public byte[] readByteArrayField(Object inst, String fieldName)
    throws FieldReaderException {
    return readByteArray(readField(inst, fieldName));
  }

  static public int readIntField(Object inst, String fieldName)
    throws FieldReaderException {
    Object val = readField(inst, fieldName);
    if (! (val instanceof Integer)) {
      throw new FieldReaderException("Expected Integer, but got " + val);
    }
    Integer ival = (Integer) val;
    return ival.intValue();
  }

  // Return the bitmap instance associated with this object, or null if there
  // is none. This works for android.graphics.Bitmap instances and their
  // underlying Byte[] instances.
  static public Instance getBitmap(Instance inst) {
    ClassObj cls = inst.getClassObj();
    if ("android.graphics.Bitmap".equals(cls.getClassName())) {
      return inst;
    }

    if (inst instanceof ArrayInstance) {
      ArrayInstance array = (ArrayInstance)inst;
      if (array.getArrayType() == Type.BYTE && inst.getHardReferences().size() == 1) {
        Instance ref = inst.getHardReferences().get(0);
        ClassObj clsref = ref.getClassObj();
        if ("android.graphics.Bitmap".equals(clsref.getClassName())) {
          return ref;
        }
      }
    }
    return null;
  }

  static public class Bitmap {
    public int width;
    public int height;
    public byte[] buffer;

    public Bitmap(int width, int height, byte[] buffer) {
      this.width = width;
      this.height = height;
      this.buffer = buffer;
    }
  }

  /**
   * Read the bitmap data for the given android.graphics.Bitmap object.
   * Returns null if the object isn't for android.graphics.Bitmap or the
   * bitmap data couldn't be read.
   */
  static public Bitmap readBitmap(Instance inst) {
    try {
      verifyIsClassInstance(inst, "android.graphics.Bitmap");
      int width = readIntField(inst, "mWidth");
      int height = readIntField(inst, "mHeight");
      byte[] buffer = readByteArrayField(inst, "mBuffer");
      return new Bitmap(width, height, buffer);
    } catch (FieldReaderException e) {
      return null;
    }
  }
}

