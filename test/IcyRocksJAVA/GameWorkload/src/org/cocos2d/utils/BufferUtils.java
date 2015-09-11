package org.cocos2d.utils;

import java.nio.ByteBuffer;
import java.nio.FloatBuffer;

import android.os.Build;

public class BufferUtils {
	/**
	 * Copy with JNI for devices prior GINGERBREAD(api 9), put for newer versions(HTC Sensation has faster put(float[]) )  
	 */
	public static void copyFloats(float[] src, int srcOffset, FloatBuffer dst, int numElements) {
		dst.put(src, srcOffset, numElements);		
	}

	public static void copy(FloatBuffer src, FloatBuffer textureCoordinates,
			int capacity) {
		// TODO Auto-generated method stub
		textureCoordinates.put(src);
	}

	public static void copy(ByteBuffer src, ByteBuffer dst, int size) {
		// TODO Auto-generated method stub
		dst.put(src);
	}

	public static ByteBuffer newUnsafeByteBuffer(int n) {
		// TODO Auto-generated method stub
		return ByteBuffer.allocate(n);
	}

	public static void disposeUnsafeByteBuffer(ByteBuffer indices) {
		// TODO release unsafebuffers?
	}

	public static void copy(float[] tmpArray, int offset, ByteBuffer texCoords, int size) {
		// TODO Auto-generated method stub
		for(int i=0; i<size; i++)
		{
			texCoords.asFloatBuffer().put(tmpArray, offset, size);
		}
	}
}
