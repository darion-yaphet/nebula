/* Copyright (c) 2024 vesoft inc. All rights reserved.
 *
 * This source code is licensed under Apache 2.0 License.
 */

#include <gtest/gtest.h>

#include "codec/RowReaderWrapper.h"
#include "codec/RowWriterV2.h"
#include "common/base/Base.h"

// Coverage for the LIST_*/SET_* container codec paths and the
// processOutOfSpace() re-layout that kicks in when a variable-length field is
// written more than once. These paths were previously untested.
namespace nebula {

using nebula::cpp2::PropertyType;

static List makeList(std::vector<Value> vals) {
  List l;
  l.values = std::move(vals);
  return l;
}

static Set makeSet(std::vector<Value> vals) {
  Set s;
  for (auto& v : vals) {
    s.values.emplace(std::move(v));
  }
  return s;
}

TEST(RowContainer, ListIntRoundTrip) {
  meta::NebulaSchemaProvider schema(1);
  schema.addField("li", PropertyType::LIST_INT);

  List li = makeList({Value(1), Value(2), Value(3), Value(-7)});

  RowWriterV2 writer(&schema);
  ASSERT_EQ(WriteResult::SUCCEEDED, writer.setValue("li", Value(li)));
  ASSERT_EQ(WriteResult::SUCCEEDED, writer.finish());

  std::string encoded = std::move(writer).moveEncodedStr();
  auto reader = RowReaderWrapper::getRowReader(&schema, encoded);

  Value v = reader->getValueByName("li");
  ASSERT_EQ(Value::Type::LIST, v.type());
  const auto& out = v.getList().values;
  ASSERT_EQ(4u, out.size());
  EXPECT_EQ(1, out[0].getInt());
  EXPECT_EQ(2, out[1].getInt());
  EXPECT_EQ(3, out[2].getInt());
  EXPECT_EQ(-7, out[3].getInt());

  // Index access must agree with name access.
  EXPECT_EQ(v, reader->getValueByIndex(0));
}

TEST(RowContainer, ListFloatRoundTrip) {
  meta::NebulaSchemaProvider schema(1);
  schema.addField("lf", PropertyType::LIST_FLOAT);

  List lf = makeList({Value(1.5), Value(2.5), Value(-3.25)});

  RowWriterV2 writer(&schema);
  ASSERT_EQ(WriteResult::SUCCEEDED, writer.setValue("lf", Value(lf)));
  ASSERT_EQ(WriteResult::SUCCEEDED, writer.finish());

  std::string encoded = std::move(writer).moveEncodedStr();
  auto reader = RowReaderWrapper::getRowReader(&schema, encoded);

  Value v = reader->getValueByName("lf");
  ASSERT_EQ(Value::Type::LIST, v.type());
  const auto& out = v.getList().values;
  ASSERT_EQ(3u, out.size());
  EXPECT_FLOAT_EQ(1.5, out[0].getFloat());
  EXPECT_FLOAT_EQ(2.5, out[1].getFloat());
  EXPECT_FLOAT_EQ(-3.25, out[2].getFloat());
}

TEST(RowContainer, ListStringRoundTrip) {
  meta::NebulaSchemaProvider schema(1);
  schema.addField("ls", PropertyType::LIST_STRING);

  List ls = makeList({Value("a"), Value("bb"), Value(""), Value("ccc")});

  RowWriterV2 writer(&schema);
  ASSERT_EQ(WriteResult::SUCCEEDED, writer.setValue("ls", Value(ls)));
  ASSERT_EQ(WriteResult::SUCCEEDED, writer.finish());

  std::string encoded = std::move(writer).moveEncodedStr();
  auto reader = RowReaderWrapper::getRowReader(&schema, encoded);

  Value v = reader->getValueByName("ls");
  ASSERT_EQ(Value::Type::LIST, v.type());
  const auto& out = v.getList().values;
  ASSERT_EQ(4u, out.size());
  EXPECT_EQ("a", out[0].getStr());
  EXPECT_EQ("bb", out[1].getStr());
  EXPECT_EQ("", out[2].getStr());
  EXPECT_EQ("ccc", out[3].getStr());
}

TEST(RowContainer, SetIntRoundTrip) {
  meta::NebulaSchemaProvider schema(1);
  schema.addField("si", PropertyType::SET_INT);

  Set si = makeSet({Value(1), Value(2), Value(3)});

  RowWriterV2 writer(&schema);
  ASSERT_EQ(WriteResult::SUCCEEDED, writer.setValue("si", Value(si)));
  ASSERT_EQ(WriteResult::SUCCEEDED, writer.finish());

  std::string encoded = std::move(writer).moveEncodedStr();
  auto reader = RowReaderWrapper::getRowReader(&schema, encoded);

  Value v = reader->getValueByName("si");
  ASSERT_EQ(Value::Type::SET, v.type());
  const auto& out = v.getSet().values;
  ASSERT_EQ(3u, out.size());
  EXPECT_EQ(1u, out.count(Value(1)));
  EXPECT_EQ(1u, out.count(Value(2)));
  EXPECT_EQ(1u, out.count(Value(3)));
}

TEST(RowContainer, SetStringRoundTrip) {
  meta::NebulaSchemaProvider schema(1);
  schema.addField("ss", PropertyType::SET_STRING);

  Set ss = makeSet({Value("x"), Value("yy"), Value("zzz")});

  RowWriterV2 writer(&schema);
  ASSERT_EQ(WriteResult::SUCCEEDED, writer.setValue("ss", Value(ss)));
  ASSERT_EQ(WriteResult::SUCCEEDED, writer.finish());

  std::string encoded = std::move(writer).moveEncodedStr();
  auto reader = RowReaderWrapper::getRowReader(&schema, encoded);

  Value v = reader->getValueByName("ss");
  ASSERT_EQ(Value::Type::SET, v.type());
  const auto& out = v.getSet().values;
  ASSERT_EQ(3u, out.size());
  EXPECT_EQ(1u, out.count(Value("x")));
  EXPECT_EQ(1u, out.count(Value("yy")));
  EXPECT_EQ(1u, out.count(Value("zzz")));
}

TEST(RowContainer, EmptyList) {
  meta::NebulaSchemaProvider schema(1);
  schema.addField("li", PropertyType::LIST_INT);

  RowWriterV2 writer(&schema);
  ASSERT_EQ(WriteResult::SUCCEEDED, writer.setValue("li", Value(List())));
  ASSERT_EQ(WriteResult::SUCCEEDED, writer.finish());

  std::string encoded = std::move(writer).moveEncodedStr();
  auto reader = RowReaderWrapper::getRowReader(&schema, encoded);

  Value v = reader->getValueByName("li");
  ASSERT_EQ(Value::Type::LIST, v.type());
  EXPECT_TRUE(v.getList().values.empty());
}

// Regression: rewriting a LIST/SET field flips outOfSpaceStr_ on, so finish()
// runs processOutOfSpace(). Before the fix that re-layout skipped LIST/SET and
// left the in-place offset dangling, yielding garbage on read.
TEST(RowContainer, ContainerRewriteTriggersReLayout) {
  meta::NebulaSchemaProvider schema(1);
  schema.addField("s", PropertyType::STRING);
  schema.addField("li", PropertyType::LIST_INT);
  schema.addField("ls", PropertyType::LIST_STRING);

  RowWriterV2 writer(&schema);
  ASSERT_EQ(WriteResult::SUCCEEDED, writer.setValue("s", Value("first")));
  ASSERT_EQ(WriteResult::SUCCEEDED, writer.setValue("li", Value(makeList({Value(1), Value(2)}))));
  ASSERT_EQ(WriteResult::SUCCEEDED,
            writer.setValue("ls", Value(makeList({Value("aa"), Value("bb")}))));

  // Rewrite every variable-length field -> out-of-space path.
  ASSERT_EQ(WriteResult::SUCCEEDED, writer.setValue("s", Value("second-longer")));
  ASSERT_EQ(WriteResult::SUCCEEDED,
            writer.setValue("li", Value(makeList({Value(10), Value(20), Value(30)}))));
  ASSERT_EQ(WriteResult::SUCCEEDED,
            writer.setValue("ls", Value(makeList({Value("cccc"), Value("d")}))));
  ASSERT_EQ(WriteResult::SUCCEEDED, writer.finish());

  std::string encoded = std::move(writer).moveEncodedStr();
  auto reader = RowReaderWrapper::getRowReader(&schema, encoded);

  EXPECT_EQ("second-longer", reader->getValueByName("s").getStr());

  const auto& li = reader->getValueByName("li").getList().values;
  ASSERT_EQ(3u, li.size());
  EXPECT_EQ(10, li[0].getInt());
  EXPECT_EQ(20, li[1].getInt());
  EXPECT_EQ(30, li[2].getInt());

  const auto& ls = reader->getValueByName("ls").getList().values;
  ASSERT_EQ(2u, ls.size());
  EXPECT_EQ("cccc", ls[0].getStr());
  EXPECT_EQ("d", ls[1].getStr());
}

// A LIST/SET rewrite combined with a plain string field must keep the string
// field intact after re-layout.
TEST(RowContainer, MixedRewriteKeepsStringIntact) {
  meta::NebulaSchemaProvider schema(1);
  schema.addField("li", PropertyType::LIST_INT);
  schema.addField("s", PropertyType::STRING);

  RowWriterV2 writer(&schema);
  ASSERT_EQ(WriteResult::SUCCEEDED, writer.setValue("li", Value(makeList({Value(1)}))));
  ASSERT_EQ(WriteResult::SUCCEEDED, writer.setValue("s", Value("keep-me")));
  // Only the list is rewritten; the string must survive the compaction.
  ASSERT_EQ(WriteResult::SUCCEEDED, writer.setValue("li", Value(makeList({Value(9), Value(8)}))));
  ASSERT_EQ(WriteResult::SUCCEEDED, writer.finish());

  std::string encoded = std::move(writer).moveEncodedStr();
  auto reader = RowReaderWrapper::getRowReader(&schema, encoded);

  EXPECT_EQ("keep-me", reader->getValueByName("s").getStr());
  const auto& li = reader->getValueByName("li").getList().values;
  ASSERT_EQ(2u, li.size());
  EXPECT_EQ(9, li[0].getInt());
  EXPECT_EQ(8, li[1].getInt());
}

TEST(RowContainer, DurationRoundTrip) {
  meta::NebulaSchemaProvider schema(1);
  schema.addField("d", PropertyType::DURATION);

  Duration du(123, 456, 7);

  RowWriterV2 writer(&schema);
  ASSERT_EQ(WriteResult::SUCCEEDED, writer.setValue("d", Value(du)));
  ASSERT_EQ(WriteResult::SUCCEEDED, writer.finish());

  std::string encoded = std::move(writer).moveEncodedStr();
  auto reader = RowReaderWrapper::getRowReader(&schema, encoded);

  Value v = reader->getValueByName("d");
  ASSERT_EQ(Value::Type::DURATION, v.type());
  EXPECT_EQ(du, v.getDuration());
}

TEST(RowContainer, NullableContainerUnset) {
  meta::NebulaSchemaProvider schema(1);
  schema.addField("li", PropertyType::LIST_INT, 0, true /*nullable*/);

  RowWriterV2 writer(&schema);
  // Field left unset; nullable -> finish() fills NULL.
  ASSERT_EQ(WriteResult::SUCCEEDED, writer.finish());

  std::string encoded = std::move(writer).moveEncodedStr();
  auto reader = RowReaderWrapper::getRowReader(&schema, encoded);

  Value v = reader->getValueByName("li");
  EXPECT_EQ(Value::Type::NULLVALUE, v.type());
}

}  // namespace nebula

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  folly::init(&argc, &argv, true);
  google::SetStderrLogging(google::INFO);

  return RUN_ALL_TESTS();
}
