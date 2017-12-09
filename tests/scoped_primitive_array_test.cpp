/*
 * Copyright (C) 2017 The Android Open Source Project
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

#include <nativehelper/scoped_primitive_array.h>

#include <gtest/gtest.h>

struct TestType { char dummy[1]; };
using jTestTypeArray = void*;

const jTestTypeArray LARGE_ARRAY = reinterpret_cast<jTestTypeArray>(0x1);
const jTestTypeArray SMALL_ARRAY = reinterpret_cast<jTestTypeArray>(0x2);

constexpr size_t LARGE_ARRAY_SIZE = 8192;
constexpr size_t SMALL_ARRAY_SIZE = 32;

struct TestContext {
    TestType* dummyPtr;

    int getArrayElementsCallCount = 0;
    int releaseArrayElementsCallCount = 0;
    bool NPEThrown = false;
    bool elementsUpdated = false;

    void resetCallCount() {
        getArrayElementsCallCount = 0;
        releaseArrayElementsCallCount = 0;
        NPEThrown = false;
        elementsUpdated = false;
    }

    bool memoryUpdated() const {
        return releaseArrayElementsCallCount > 0 && elementsUpdated;
    }
};

// Mock implementation of the ScopedPrimitiveArrayTraits.
// JNIEnv is abused for passing TestContext.
template<> struct ScopedPrimitiveArrayTraits<TestType> {
public:
    static inline void getArrayRegion(JNIEnv*, jTestTypeArray, size_t, size_t, TestType*) {}

    static inline TestType* getArrayElements(JNIEnv* env, jTestTypeArray) {
        TestContext* ctx = reinterpret_cast<TestContext*>(env);
        ctx->getArrayElementsCallCount++;
        return ctx->dummyPtr;
    }

    static inline void releaseArrayElements(JNIEnv* env, jTestTypeArray, TestType* buffer,
                                            jint mode) {
        TestContext* ctx = reinterpret_cast<TestContext*>(env);
        if (ctx->dummyPtr == buffer) {
            ctx->releaseArrayElementsCallCount++;
        }
        ctx->elementsUpdated = (mode != JNI_ABORT);
    }

    static inline size_t getArrayLength(JNIEnv*, jTestTypeArray array) {
        return array == LARGE_ARRAY ? LARGE_ARRAY_SIZE : SMALL_ARRAY_SIZE;
    }

    static inline void throwNullPointerException(JNIEnv* env) {
        reinterpret_cast<TestContext*>(env)->NPEThrown = true;
    }

    using ArrayType = jTestTypeArray;
};

TEST(ScopedPrimitiveArrayTest, testNonNullArray) {
    std::unique_ptr<TestType[]> dummyTestType = std::make_unique<TestType[]>(LARGE_ARRAY_SIZE);

    TestContext context;
    context.dummyPtr = dummyTestType.get();

    JNIEnv* env = reinterpret_cast<JNIEnv*>(&context);
    {
        context.resetCallCount();
        {
            ScopedArrayRO<TestType, false /* non null */> array(env, SMALL_ARRAY);
            EXPECT_NE(nullptr, array.get());
            EXPECT_EQ(SMALL_ARRAY, array.getJavaArray());
            EXPECT_NE(nullptr, array.begin());
            EXPECT_NE(nullptr, array.end());
            EXPECT_EQ(array.end(), array.begin() + SMALL_ARRAY_SIZE);
            EXPECT_EQ(SMALL_ARRAY_SIZE, array.size());
        }
        EXPECT_EQ(context.getArrayElementsCallCount, context.releaseArrayElementsCallCount);
        EXPECT_FALSE(context.memoryUpdated());
        EXPECT_FALSE(context.NPEThrown);
    }
    {
        context.resetCallCount();
        {
            ScopedArrayRO<TestType, false /* non null */> array(env, LARGE_ARRAY);

            EXPECT_EQ(context.dummyPtr, array.get());
            EXPECT_EQ(LARGE_ARRAY, array.getJavaArray());
            EXPECT_EQ(context.dummyPtr, array.begin());
            EXPECT_EQ(context.dummyPtr + LARGE_ARRAY_SIZE, array.end());
            EXPECT_EQ(LARGE_ARRAY_SIZE, array.size());
        }
        EXPECT_EQ(context.getArrayElementsCallCount, context.releaseArrayElementsCallCount);
        EXPECT_FALSE(context.memoryUpdated());
        EXPECT_FALSE(context.NPEThrown);
    }
    {
        context.resetCallCount();
        {
            ScopedArrayRO<TestType, false /* non null */> array(env, nullptr);
            EXPECT_TRUE(context.NPEThrown);
        }
    }
}

TEST(ScopedPrimitiveArrayTest, testNullableArray) {
    std::unique_ptr<TestType[]> dummyTestType = std::make_unique<TestType[]>(LARGE_ARRAY_SIZE);

    TestContext context;
    context.dummyPtr = dummyTestType.get();

    JNIEnv* env = reinterpret_cast<JNIEnv*>(&context);
    {
        context.resetCallCount();
        {
            ScopedArrayRO<TestType, true /* nullable */> array(env, SMALL_ARRAY);
            EXPECT_NE(nullptr, array.get());
            EXPECT_EQ(SMALL_ARRAY, array.getJavaArray());
            EXPECT_NE(nullptr, array.begin());
            EXPECT_NE(nullptr, array.end());
            EXPECT_EQ(array.end(), array.begin() + SMALL_ARRAY_SIZE);
            EXPECT_EQ(SMALL_ARRAY_SIZE, (size_t) array.size());
        }
        EXPECT_EQ(context.getArrayElementsCallCount, context.releaseArrayElementsCallCount);
        EXPECT_FALSE(context.memoryUpdated());
        EXPECT_FALSE(context.NPEThrown);
    }
    {
        context.resetCallCount();
        {
            ScopedArrayRO<TestType, true /* nullable */> array(env, LARGE_ARRAY);
            EXPECT_EQ(context.dummyPtr, array.get());
            EXPECT_EQ(LARGE_ARRAY, array.getJavaArray());
            EXPECT_EQ(context.dummyPtr, array.begin());
            EXPECT_EQ(context.dummyPtr + LARGE_ARRAY_SIZE, array.end());
            EXPECT_EQ(LARGE_ARRAY_SIZE, (size_t) array.size());
        }
        EXPECT_EQ(context.getArrayElementsCallCount, context.releaseArrayElementsCallCount);
        EXPECT_FALSE(context.memoryUpdated());
        EXPECT_FALSE(context.NPEThrown);
    }
    {
        context.resetCallCount();
        {
            ScopedArrayRO<TestType, true /* nullable*/> array(env, nullptr);
            EXPECT_EQ(nullptr, array.get());
            EXPECT_EQ(nullptr, array.getJavaArray());
            EXPECT_EQ(nullptr, array.begin());
            EXPECT_EQ(nullptr, array.end());
            EXPECT_EQ(-1, array.size());
        }
        EXPECT_EQ(context.getArrayElementsCallCount, context.releaseArrayElementsCallCount);
        EXPECT_FALSE(context.memoryUpdated());
        EXPECT_FALSE(context.NPEThrown);
    }
}

TEST(ScopedPrimitiveArrayTest, testArrayRW) {
    std::unique_ptr<TestType[]> dummyTestType = std::make_unique<TestType[]>(LARGE_ARRAY_SIZE);

    TestContext context;
    context.dummyPtr = dummyTestType.get();

    JNIEnv* env = reinterpret_cast<JNIEnv*>(&context);
    {
        context.resetCallCount();
        {
            ScopedArrayRW<TestType> array(env, SMALL_ARRAY);
            EXPECT_NE(nullptr, array.get());
            EXPECT_EQ(SMALL_ARRAY, array.getJavaArray());
            EXPECT_NE(nullptr, array.begin());
            EXPECT_NE(nullptr, array.end());
            EXPECT_EQ(array.end(), array.begin() + SMALL_ARRAY_SIZE);
            EXPECT_EQ(SMALL_ARRAY_SIZE, (size_t) array.size());
        }
        EXPECT_EQ(context.getArrayElementsCallCount, context.releaseArrayElementsCallCount);
        EXPECT_TRUE(context.memoryUpdated());
        EXPECT_FALSE(context.NPEThrown);
    }
    {
        context.resetCallCount();
        {
            ScopedArrayRW<TestType> array(env, LARGE_ARRAY);
            EXPECT_EQ(context.dummyPtr, array.get());
            EXPECT_EQ(LARGE_ARRAY, array.getJavaArray());
            EXPECT_EQ(context.dummyPtr, array.begin());
            EXPECT_EQ(context.dummyPtr + LARGE_ARRAY_SIZE, array.end());
            EXPECT_EQ(LARGE_ARRAY_SIZE, (size_t) array.size());
        }
        EXPECT_EQ(context.getArrayElementsCallCount, context.releaseArrayElementsCallCount);
        EXPECT_TRUE(context.memoryUpdated());
        EXPECT_FALSE(context.NPEThrown);
    }
    {
        context.resetCallCount();
        {
            ScopedArrayRW<TestType> array(env, nullptr);
            EXPECT_TRUE(context.NPEThrown);
        }
    }
}
