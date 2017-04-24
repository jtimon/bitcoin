// Copyright (c) 2012-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "rpc/server.h"
#include "rpc/client.h"

#include "base58.h"
#include "netbase.h"

#include "test/test_bitcoin.h"

#include <boost/algorithm/string.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/test/unit_test.hpp>

#include <univalue.h>

UniValue CallRPC(std::string args)
{
    std::vector<std::string> vArgs;
    boost::split(vArgs, args, boost::is_any_of(" \t"));
    std::string strMethod = vArgs[0];
    vArgs.erase(vArgs.begin());
    JSONRPCRequest request;
    request.strMethod = strMethod;
    request.params = RPCConvertValues(strMethod, vArgs);
    request.fHelp = false;
    BOOST_CHECK(tableRPC[strMethod]);
    rpcfn_type method = tableRPC[strMethod]->actor;
    try {
        UniValue result = (*method)(request);
        return result;
    }
    catch (const UniValue& objError) {
        throw std::runtime_error(find_value(objError, "message").get_str());
    }
}


BOOST_FIXTURE_TEST_SUITE(rpc_tests, TestingSetup)

BOOST_AUTO_TEST_CASE(rpc_rawparams)
{
    // Test raw transaction API argument handling
    UniValue r;

    BOOST_CHECK_THROW(CallRPC("getrawtransaction"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("getrawtransaction not_hex"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("getrawtransaction a3b807410df0b60fcb9736768df5823938b2f838694939ba45f3c0a1bff150ed not_int"), std::runtime_error);

    BOOST_CHECK_THROW(CallRPC("createrawtransaction"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("createrawtransaction null null"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("createrawtransaction not_array"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("createrawtransaction [] []"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("createrawtransaction {} {}"), std::runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC("createrawtransaction [] {}"));
    BOOST_CHECK_THROW(CallRPC("createrawtransaction [] {} extra"), std::runtime_error);

    BOOST_CHECK_THROW(CallRPC("decoderawtransaction"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("decoderawtransaction null"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("decoderawtransaction DEADBEEF"), std::runtime_error);
    std::string rawtx = "0100000001a15d57094aa7a21a28cb20b59aab8fc7d1149a3bdbcddba9c622e4f5f6a99ece010000006c493046022100f93bb0e7d8db7bd46e40132d1f8242026e045f03a0efe71bbb8e3f475e970d790221009337cd7f1f929f00cc6ff01f03729b069a7c21b59b1736ddfee5db5946c5da8c0121033b9b137ee87d5a812d6f506efdd37f0affa7ffc310711c06c7f3e097c9447c52ffffffff0100e1f505000000001976a9140389035a9225b3839e2bbf32d826a1e222031fd888ac00000000";
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("decoderawtransaction ")+rawtx));
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "size").get_int(), 193);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "version").get_int(), 1);
    BOOST_CHECK_EQUAL(find_value(r.get_obj(), "locktime").get_int(), 0);
    BOOST_CHECK_THROW(r = CallRPC(std::string("decoderawtransaction ")+rawtx+" extra"), std::runtime_error);

    BOOST_CHECK_THROW(CallRPC("signrawtransaction"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("signrawtransaction null"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("signrawtransaction ff00"), std::runtime_error);
    BOOST_CHECK_NO_THROW(CallRPC(std::string("signrawtransaction ")+rawtx));
    BOOST_CHECK_NO_THROW(CallRPC(std::string("signrawtransaction ")+rawtx+" null null NONE|ANYONECANPAY"));
    BOOST_CHECK_NO_THROW(CallRPC(std::string("signrawtransaction ")+rawtx+" [] [] NONE|ANYONECANPAY"));
    BOOST_CHECK_THROW(CallRPC(std::string("signrawtransaction ")+rawtx+" null null badenum"), std::runtime_error);

    // Only check failure cases for sendrawtransaction, there's no network to send to...
    BOOST_CHECK_THROW(CallRPC("sendrawtransaction"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("sendrawtransaction null"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("sendrawtransaction DEADBEEF"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC(std::string("sendrawtransaction ")+rawtx+" extra"), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(rpc_togglenetwork)
{
    UniValue r;

    r = CallRPC("getnetworkinfo");
    bool netState = find_value(r.get_obj(), "networkactive").get_bool();
    BOOST_CHECK_EQUAL(netState, true);

    BOOST_CHECK_NO_THROW(CallRPC("setnetworkactive false"));
    r = CallRPC("getnetworkinfo");
    int numConnection = find_value(r.get_obj(), "connections").get_int();
    BOOST_CHECK_EQUAL(numConnection, 0);

    netState = find_value(r.get_obj(), "networkactive").get_bool();
    BOOST_CHECK_EQUAL(netState, false);

    BOOST_CHECK_NO_THROW(CallRPC("setnetworkactive true"));
    r = CallRPC("getnetworkinfo");
    netState = find_value(r.get_obj(), "networkactive").get_bool();
    BOOST_CHECK_EQUAL(netState, true);
}

BOOST_AUTO_TEST_CASE(rpc_rawsign)
{
    UniValue r;
    // input is a 1-of-2 multisig (so is output):
    std::string prevout =
      "[{\"txid\":\"b4cc287e58f87cdae59417329f710f3ecd75a4ee1d2872b7248f50977c8493f3\","
      "\"vout\":1,\"scriptPubKey\":\"a914b10c9df5f7edf436c697f02f1efdba4cf399615187\","
      "\"redeemScript\":\"512103debedc17b3df2badbcdd86d5feb4562b86fe182e5998abd8bcd4f122c6155b1b21027e940bb73ab8732bfdf7f9216ecefca5b94d6df834e77e108f68e66f126044c052ae\"}]";
    r = CallRPC(std::string("createrawtransaction ")+prevout+" "+
      "{\"3HqAe9LtNBjnsfM4CyYaWTnvCaUYT7v4oZ\":11}");
    std::string notsigned = r.get_str();
    std::string privkey1 = "\"KzsXybp9jX64P5ekX1KUxRQ79Jht9uzW7LorgwE65i5rWACL6LQe\"";
    std::string privkey2 = "\"Kyhdf5LuKTRx4ge69ybABsiUAWjVRK4XGxAKk2FQLp2HjGMy87Z4\"";
    r = CallRPC(std::string("signrawtransaction ")+notsigned+" "+prevout+" "+"[]");
    BOOST_CHECK(find_value(r.get_obj(), "complete").get_bool() == false);
    r = CallRPC(std::string("signrawtransaction ")+notsigned+" "+prevout+" "+"["+privkey1+","+privkey2+"]");
    BOOST_CHECK(find_value(r.get_obj(), "complete").get_bool() == true);
}

BOOST_AUTO_TEST_CASE(rpc_createraw_op_return)
{
    BOOST_CHECK_NO_THROW(CallRPC("createrawtransaction [{\"txid\":\"a3b807410df0b60fcb9736768df5823938b2f838694939ba45f3c0a1bff150ed\",\"vout\":0}] {\"data\":\"68656c6c6f776f726c64\"}"));

    // Allow more than one data transaction output
    BOOST_CHECK_NO_THROW(CallRPC("createrawtransaction [{\"txid\":\"a3b807410df0b60fcb9736768df5823938b2f838694939ba45f3c0a1bff150ed\",\"vout\":0}] {\"data\":\"68656c6c6f776f726c64\",\"data\":\"68656c6c6f776f726c64\"}"));

    // Key not "data" (bad address)
    BOOST_CHECK_THROW(CallRPC("createrawtransaction [{\"txid\":\"a3b807410df0b60fcb9736768df5823938b2f838694939ba45f3c0a1bff150ed\",\"vout\":0}] {\"somedata\":\"68656c6c6f776f726c64\"}"), std::runtime_error);

    // Bad hex encoding of data output
    BOOST_CHECK_THROW(CallRPC("createrawtransaction [{\"txid\":\"a3b807410df0b60fcb9736768df5823938b2f838694939ba45f3c0a1bff150ed\",\"vout\":0}] {\"data\":\"12345\"}"), std::runtime_error);
    BOOST_CHECK_THROW(CallRPC("createrawtransaction [{\"txid\":\"a3b807410df0b60fcb9736768df5823938b2f838694939ba45f3c0a1bff150ed\",\"vout\":0}] {\"data\":\"12345g\"}"), std::runtime_error);

    // Data 81 bytes long
    BOOST_CHECK_NO_THROW(CallRPC("createrawtransaction [{\"txid\":\"a3b807410df0b60fcb9736768df5823938b2f838694939ba45f3c0a1bff150ed\",\"vout\":0}] {\"data\":\"010203040506070809101112131415161718192021222324252627282930313233343536373839404142434445464748495051525354555657585960616263646566676869707172737475767778798081\"}"));
}

BOOST_AUTO_TEST_CASE(rpc_format_monetary_values)
{
    BOOST_CHECK(ValueFromAmountDecimals(0LL, false).write() == "0");
    BOOST_CHECK(ValueFromAmountDecimals(1LL, false).write() == "1");
    BOOST_CHECK(ValueFromAmountDecimals(17622195LL, false).write() == "17622195");
    BOOST_CHECK(ValueFromAmountDecimals(50000000LL, false).write() == "50000000");
    BOOST_CHECK(ValueFromAmountDecimals(89898989LL, false).write() == "89898989");
    BOOST_CHECK(ValueFromAmountDecimals(100000000LL, false).write() == "100000000"); // 1 CURRENCY_UNIT
    BOOST_CHECK(ValueFromAmountDecimals(100000000000000LL, false).write() == "100000000000000"); // 1 M CURRENCY_UNIT
    BOOST_CHECK(ValueFromAmountDecimals(2100000000000000LL, false).write() == "2100000000000000"); // 21 M CURRENCY_UNIT
    BOOST_CHECK(ValueFromAmountDecimals(10000000000000000LL, false).write() == "10000000000000000");  // 100 M CURRENCY_UNIT (100 000 000 0000 0000 MINIMAL_UNIT)
    BOOST_CHECK(ValueFromAmountDecimals(2099999999999990LL, false).write() == "2099999999999990");
    BOOST_CHECK(ValueFromAmountDecimals(2099999999999999LL, false).write() == "2099999999999999");

    BOOST_CHECK(ValueFromAmountDecimals(0LL, true).write() == "0.00000000");
    BOOST_CHECK(ValueFromAmountDecimals(1LL, true).write() == "0.00000001");
    BOOST_CHECK(ValueFromAmountDecimals(17622195LL, true).write() == "0.17622195");
    BOOST_CHECK(ValueFromAmountDecimals(50000000LL, true).write() == "0.50000000");
    BOOST_CHECK(ValueFromAmountDecimals(89898989LL, true).write() == "0.89898989");
    BOOST_CHECK(ValueFromAmountDecimals(100000000LL, true).write() == "1.00000000");
    BOOST_CHECK(ValueFromAmountDecimals(2099999999999990LL, true).write() == "20999999.99999990");
    BOOST_CHECK(ValueFromAmountDecimals(2099999999999999LL, true).write() == "20999999.99999999");

    BOOST_CHECK_EQUAL(ValueFromAmountDecimals(0, true).write(), "0.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmountDecimals((COIN/10000)*123456789, true).write(), "12345.67890000");
    BOOST_CHECK_EQUAL(ValueFromAmountDecimals(-COIN, true).write(), "-1.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmountDecimals(-COIN/10, true).write(), "-0.10000000");

    BOOST_CHECK_EQUAL(ValueFromAmountDecimals(COIN*100000000, true).write(), "100000000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmountDecimals(COIN*10000000, true).write(), "10000000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmountDecimals(COIN*1000000, true).write(), "1000000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmountDecimals(COIN*100000, true).write(), "100000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmountDecimals(COIN*10000, true).write(), "10000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmountDecimals(COIN*1000, true).write(), "1000.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmountDecimals(COIN*100, true).write(), "100.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmountDecimals(COIN*10, true).write(), "10.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmountDecimals(COIN, true).write(), "1.00000000");
    BOOST_CHECK_EQUAL(ValueFromAmountDecimals(COIN/10, true).write(), "0.10000000");
    BOOST_CHECK_EQUAL(ValueFromAmountDecimals(COIN/100, true).write(), "0.01000000");
    BOOST_CHECK_EQUAL(ValueFromAmountDecimals(COIN/1000, true).write(), "0.00100000");
    BOOST_CHECK_EQUAL(ValueFromAmountDecimals(COIN/10000, true).write(), "0.00010000");
    BOOST_CHECK_EQUAL(ValueFromAmountDecimals(COIN/100000, true).write(), "0.00001000");
    BOOST_CHECK_EQUAL(ValueFromAmountDecimals(COIN/1000000, true).write(), "0.00000100");
    BOOST_CHECK_EQUAL(ValueFromAmountDecimals(COIN/10000000, true).write(), "0.00000010");
    BOOST_CHECK_EQUAL(ValueFromAmountDecimals(COIN/100000000, true).write(), "0.00000001");
}

static UniValue ValueFromString(const std::string &str)
{
    UniValue value;
    BOOST_CHECK(value.setNumStr(str));
    return value;
}

BOOST_AUTO_TEST_CASE(rpc_parse_monetary_values)
{
    BOOST_CHECK_THROW(AmountFromValueDecimals(ValueFromString("-1"), false), UniValue);
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("0"), false), 0LL);
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("1"), false), 1LL);
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("17622195"), false), 17622195LL);
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("50000000"), false), 50000000LL);
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("89898989"), false), 89898989LL);
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("100000000"), false),  100000000LL); // 1 CURRENCY_UNIT
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("100000000000000"), false), 100000000000000LL); // 1 M CURRENCY_UNIT
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("2100000000000000"), false), 2100000000000000LL); // 21 M CURRENCY_UNIT
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("2099999999999999"), false), 2099999999999999LL);
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("2099999999999990"), false), 2099999999999990LL);
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("209999999999999"), false), 209999999999999LL);
    // FIX Decimals: This shouldn't fail? or make sure it always fails
    // BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("10000000000000000"), false), 10000000000000000LL); // 100 M CURRENCY_UNIT (100 000 000 0000 0000 MINIMAL_UNIT)

    BOOST_CHECK_THROW(AmountFromValueDecimals(ValueFromString("-0.00000001"), true), UniValue);
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("0"), true), 0LL);
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("0.00000000"), true), 0LL);
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("0.00000001"), true), 1LL);
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("0.17622195"), true), 17622195LL);
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("0.5"), true), 50000000LL);
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("0.50000000"), true), 50000000LL);
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("0.89898989"), true), 89898989LL);
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("1.00000000"), true), 100000000LL);
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("20999999.9999999"), true), 2099999999999990LL);
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("20999999.99999999"), true), 2099999999999999LL);

    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("1e-8"), true), COIN/100000000);
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("0.1e-7"), true), COIN/100000000);
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("0.01e-6"), true), COIN/100000000);
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("0.0000000000000000000000000000000000000000000000000000000000000000000000000001e+68"), true), COIN/100000000);
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("10000000000000000000000000000000000000000000000000000000000000000e-64"), true), COIN);
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("0.000000000000000000000000000000000000000000000000000000000000000100000000000000000000000000000000000000000000000000000e64"), true), COIN);

    BOOST_CHECK_THROW(AmountFromValueDecimals(ValueFromString("1e-9"), true), UniValue); //should fail
    BOOST_CHECK_THROW(AmountFromValueDecimals(ValueFromString("0.000000019"), true), UniValue); //should fail
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("0.00000001000000"), true), 1LL); //should pass, cut trailing 0
    BOOST_CHECK_THROW(AmountFromValueDecimals(ValueFromString("19e-9"), true), UniValue); //should fail
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ValueFromString("0.19e-6"), true), 19); //should pass, leading 0 is present

    BOOST_CHECK_THROW(AmountFromValueDecimals(ValueFromString("92233720368.54775808"), true), UniValue); //overflow error
    BOOST_CHECK_THROW(AmountFromValueDecimals(ValueFromString("1e+11"), true), UniValue); //overflow error
    BOOST_CHECK_THROW(AmountFromValueDecimals(ValueFromString("1e11"), true), UniValue); //overflow error signless
    BOOST_CHECK_THROW(AmountFromValueDecimals(ValueFromString("93e+9"), true), UniValue); //overflow error
}

BOOST_AUTO_TEST_CASE(json_parse_errors)
{
    // Valid
    BOOST_CHECK_EQUAL(ParseNonRFCJSONValue("1.0").get_real(), 1.0);
    // Valid, with leading or trailing whitespace
    BOOST_CHECK_EQUAL(ParseNonRFCJSONValue(" 1.0").get_real(), 1.0);
    BOOST_CHECK_EQUAL(ParseNonRFCJSONValue("1.0 ").get_real(), 1.0);

    BOOST_CHECK_THROW(AmountFromValueDecimals(ParseNonRFCJSONValue(".19e-6"), true), std::runtime_error); //should fail, missing leading 0, therefore invalid JSON
    BOOST_CHECK_EQUAL(AmountFromValueDecimals(ParseNonRFCJSONValue("0.00000000000000000000000000000000000001e+30 "), true), 1);
    // Invalid, initial garbage
    BOOST_CHECK_THROW(ParseNonRFCJSONValue("[1.0"), std::runtime_error);
    BOOST_CHECK_THROW(ParseNonRFCJSONValue("a1.0"), std::runtime_error);
    // Invalid, trailing garbage
    BOOST_CHECK_THROW(ParseNonRFCJSONValue("1.0sds"), std::runtime_error);
    BOOST_CHECK_THROW(ParseNonRFCJSONValue("1.0]"), std::runtime_error);
    // BTC addresses should fail parsing
    BOOST_CHECK_THROW(ParseNonRFCJSONValue("175tWpb8K1S7NmH4Zx6rewF9WQrcZv245W"), std::runtime_error);
    BOOST_CHECK_THROW(ParseNonRFCJSONValue("3J98t1WpEZ73CNmQviecrnyiWrnqRhWNL"), std::runtime_error);
}

BOOST_AUTO_TEST_CASE(rpc_ban)
{
    BOOST_CHECK_NO_THROW(CallRPC(std::string("clearbanned")));

    UniValue r;
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("setban 127.0.0.0 add")));
    BOOST_CHECK_THROW(r = CallRPC(std::string("setban 127.0.0.0:8334")), std::runtime_error); //portnumber for setban not allowed
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("listbanned")));
    UniValue ar = r.get_array();
    UniValue o1 = ar[0].get_obj();
    UniValue adr = find_value(o1, "address");
    BOOST_CHECK_EQUAL(adr.get_str(), "127.0.0.0/32");
    BOOST_CHECK_NO_THROW(CallRPC(std::string("setban 127.0.0.0 remove")));
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("listbanned")));
    ar = r.get_array();
    BOOST_CHECK_EQUAL(ar.size(), 0);

    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("setban 127.0.0.0/24 add 1607731200 true")));
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("listbanned")));
    ar = r.get_array();
    o1 = ar[0].get_obj();
    adr = find_value(o1, "address");
    UniValue banned_until = find_value(o1, "banned_until");
    BOOST_CHECK_EQUAL(adr.get_str(), "127.0.0.0/24");
    BOOST_CHECK_EQUAL(banned_until.get_int64(), 1607731200); // absolute time check

    BOOST_CHECK_NO_THROW(CallRPC(std::string("clearbanned")));

    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("setban 127.0.0.0/24 add 200")));
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("listbanned")));
    ar = r.get_array();
    o1 = ar[0].get_obj();
    adr = find_value(o1, "address");
    banned_until = find_value(o1, "banned_until");
    BOOST_CHECK_EQUAL(adr.get_str(), "127.0.0.0/24");
    int64_t now = GetTime();
    BOOST_CHECK(banned_until.get_int64() > now);
    BOOST_CHECK(banned_until.get_int64()-now <= 200);

    // must throw an exception because 127.0.0.1 is in already banned suubnet range
    BOOST_CHECK_THROW(r = CallRPC(std::string("setban 127.0.0.1 add")), std::runtime_error);

    BOOST_CHECK_NO_THROW(CallRPC(std::string("setban 127.0.0.0/24 remove")));
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("listbanned")));
    ar = r.get_array();
    BOOST_CHECK_EQUAL(ar.size(), 0);

    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("setban 127.0.0.0/255.255.0.0 add")));
    BOOST_CHECK_THROW(r = CallRPC(std::string("setban 127.0.1.1 add")), std::runtime_error);

    BOOST_CHECK_NO_THROW(CallRPC(std::string("clearbanned")));
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("listbanned")));
    ar = r.get_array();
    BOOST_CHECK_EQUAL(ar.size(), 0);


    BOOST_CHECK_THROW(r = CallRPC(std::string("setban test add")), std::runtime_error); //invalid IP

    //IPv6 tests
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("setban FE80:0000:0000:0000:0202:B3FF:FE1E:8329 add")));
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("listbanned")));
    ar = r.get_array();
    o1 = ar[0].get_obj();
    adr = find_value(o1, "address");
    BOOST_CHECK_EQUAL(adr.get_str(), "fe80::202:b3ff:fe1e:8329/128");

    BOOST_CHECK_NO_THROW(CallRPC(std::string("clearbanned")));
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("setban 2001:db8::/ffff:fffc:0:0:0:0:0:0 add")));
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("listbanned")));
    ar = r.get_array();
    o1 = ar[0].get_obj();
    adr = find_value(o1, "address");
    BOOST_CHECK_EQUAL(adr.get_str(), "2001:db8::/30");

    BOOST_CHECK_NO_THROW(CallRPC(std::string("clearbanned")));
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("setban 2001:4d48:ac57:400:cacf:e9ff:fe1d:9c63/128 add")));
    BOOST_CHECK_NO_THROW(r = CallRPC(std::string("listbanned")));
    ar = r.get_array();
    o1 = ar[0].get_obj();
    adr = find_value(o1, "address");
    BOOST_CHECK_EQUAL(adr.get_str(), "2001:4d48:ac57:400:cacf:e9ff:fe1d:9c63/128");
}

BOOST_AUTO_TEST_CASE(rpc_convert_values_generatetoaddress)
{
    UniValue result;

    BOOST_CHECK_NO_THROW(result = RPCConvertValues("generatetoaddress", boost::assign::list_of("101")("mkESjLZW66TmHhiFX8MCaBjrhZ543PPh9a")));
    BOOST_CHECK_EQUAL(result[0].get_int(), 101);
    BOOST_CHECK_EQUAL(result[1].get_str(), "mkESjLZW66TmHhiFX8MCaBjrhZ543PPh9a");

    BOOST_CHECK_NO_THROW(result = RPCConvertValues("generatetoaddress", boost::assign::list_of("101")("mhMbmE2tE9xzJYCV9aNC8jKWN31vtGrguU")));
    BOOST_CHECK_EQUAL(result[0].get_int(), 101);
    BOOST_CHECK_EQUAL(result[1].get_str(), "mhMbmE2tE9xzJYCV9aNC8jKWN31vtGrguU");

    BOOST_CHECK_NO_THROW(result = RPCConvertValues("generatetoaddress", boost::assign::list_of("1")("mkESjLZW66TmHhiFX8MCaBjrhZ543PPh9a")("9")));
    BOOST_CHECK_EQUAL(result[0].get_int(), 1);
    BOOST_CHECK_EQUAL(result[1].get_str(), "mkESjLZW66TmHhiFX8MCaBjrhZ543PPh9a");
    BOOST_CHECK_EQUAL(result[2].get_int(), 9);

    BOOST_CHECK_NO_THROW(result = RPCConvertValues("generatetoaddress", boost::assign::list_of("1")("mhMbmE2tE9xzJYCV9aNC8jKWN31vtGrguU")("9")));
    BOOST_CHECK_EQUAL(result[0].get_int(), 1);
    BOOST_CHECK_EQUAL(result[1].get_str(), "mhMbmE2tE9xzJYCV9aNC8jKWN31vtGrguU");
    BOOST_CHECK_EQUAL(result[2].get_int(), 9);
}

BOOST_AUTO_TEST_SUITE_END()
