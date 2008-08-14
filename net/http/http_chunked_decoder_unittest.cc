// Copyright 2008, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "base/basictypes.h"
#include "net/http/http_chunked_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

typedef testing::Test HttpChunkedDecoderTest;

void RunTest(const char* inputs[], size_t num_inputs,
             const char* expected_output,
             bool expected_eof) {
  net::HttpChunkedDecoder decoder;
  EXPECT_FALSE(decoder.reached_eof());

  std::string result;

  for (size_t i = 0; i < num_inputs; ++i) {
    std::string input = inputs[i];
    int n = decoder.FilterBuf(&input[0], static_cast<int>(input.size()));
    EXPECT_GE(n, 0);
    if (n > 0)
      result.append(input.data(), n);
  }

  EXPECT_TRUE(result == expected_output);
  EXPECT_TRUE(decoder.reached_eof() == expected_eof);
}

// Feed the inputs to the decoder, until it returns an error.
void RunTestUntilFailure(const char* inputs[], size_t num_inputs, size_t fail_index) {
  net::HttpChunkedDecoder decoder;
  EXPECT_FALSE(decoder.reached_eof());

  for (size_t i = 0; i < num_inputs; ++i) {
    std::string input = inputs[i];
    int n = decoder.FilterBuf(&input[0], static_cast<int>(input.size()));
    if (n < 0) {
      EXPECT_EQ(fail_index, i);
      return;
    }
  }
  FAIL(); // We should have failed on the i'th iteration of the loop.
}

}  // namespace

TEST(HttpChunkedDecoderTest, Basic) {
  const char* inputs[] = {
    "5\r\nhello\r\n0\r\n\r\n"
  };
  RunTest(inputs, arraysize(inputs), "hello", true);
}

TEST(HttpChunkedDecoderTest, OneChunk) {
  const char* inputs[] = {
    "5\r\nhello\r\n"
  };
  RunTest(inputs, arraysize(inputs), "hello", false);
}

TEST(HttpChunkedDecoderTest, Typical) {
  const char* inputs[] = {
    "5\r\nhello\r\n",
    "1\r\n \r\n",
    "5\r\nworld\r\n",
    "0\r\n\r\n"
  };
  RunTest(inputs, arraysize(inputs), "hello world", true);
}

TEST(HttpChunkedDecoderTest, Incremental) {
  const char* inputs[] = {
    "5",
    "\r",
    "\n",
    "hello",
    "\r",
    "\n",
    "0",
    "\r",
    "\n",
    "\r",
    "\n"
  };
  RunTest(inputs, arraysize(inputs), "hello", true);
}

TEST(HttpChunkedDecoderTest, Extensions) {
  const char* inputs[] = {
    "5;x=0\r\nhello\r\n",
    "0;y=\"2 \"\r\n\r\n"
  };
  RunTest(inputs, arraysize(inputs), "hello", true);
}

TEST(HttpChunkedDecoderTest, Trailers) {
  const char* inputs[] = {
    "5\r\nhello\r\n",
    "0\r\n",
    "Foo: 1\r\n",
    "Bar: 2\r\n",
    "\r\n"
  };
  RunTest(inputs, arraysize(inputs), "hello", true);
}

TEST(HttpChunkedDecoderTest, TrailersUnfinished) {
  const char* inputs[] = {
    "5\r\nhello\r\n",
    "0\r\n",
    "Foo: 1\r\n"
  };
  RunTest(inputs, arraysize(inputs), "hello", false);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_0X) {
  const char* inputs[] = {
    "0x5\r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, arraysize(inputs), 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_TrailingWhitespace) {
  const char* inputs[] = {
    "5 \r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, arraysize(inputs), 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_LeadingWhitespace) {
  const char* inputs[] = {
    " 5\r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, arraysize(inputs), 0);
}

TEST(HttpChunkedDecoderTest, InvalidLeadingSeparator) {
  const char* inputs[] = {
    "\r\n5\r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, arraysize(inputs), 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_NoSeparator) {
  const char* inputs[] = {
    "5\r\nhello",
    "1\r\n \r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, arraysize(inputs), 1);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_Negative) {
  const char* inputs[] = {
    "8\r\n12345678\r\n-5\r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, arraysize(inputs), 0);
}

TEST(HttpChunkedDecoderTest, InvalidChunkSize_Plus) {
  const char* inputs[] = {
    "+5\r\nhello\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, arraysize(inputs), 0);
}

TEST(HttpChunkedDecoderTest, InvalidConsecutiveCRLFs) {
  const char* inputs[] = {
    "5\r\nhello\r\n",
    "\r\n\r\n\r\n\r\n",
    "0\r\n\r\n"
  };
  RunTestUntilFailure(inputs, arraysize(inputs), 1);
}