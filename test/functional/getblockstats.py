#!/usr/bin/env python3
# Copyright (c) 2017-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

#
# Test getblockstats rpc call
#
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
)

def assert_contains(data, values, check_cointains=True):
    for val in values:
        if check_cointains:
            assert val in data
        else:
            assert val not in data

class GetblockstatsTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.extra_args = [['-txindex'], ['-paytxfee=0.003']]
        self.setup_clean_chain = True

    def run_test(self):
        node = self.nodes[0]
        node.generate(101)

        node.sendtoaddress(address=self.nodes[1].getnewaddress(), amount=10, subtractfeefromamount=True)
        node.generate(1)
        self.sync_all()

        node.sendtoaddress(address=node.getnewaddress(), amount=10, subtractfeefromamount=True)
        node.sendtoaddress(address=node.getnewaddress(), amount=10, subtractfeefromamount=True)
        self.nodes[1].sendtoaddress(address=node.getnewaddress(), amount=1, subtractfeefromamount=True)
        self.sync_all()
        node.generate(1)

        start_height = 101
        max_stat_pos = 2
        stats = [node.getblockstats(height=start_height + i) for i in range(max_stat_pos+1)]

        # Make sure all valid statistics are included but nothing else is
        assert_equal(set(stats[0].keys()), {
            "height",
            "time",
            "mediantime",
            "txs",
            "swtxs",
            "ins",
            "outs",
            "subsidy",
            "totalfee",
            "utxo_increase",
            "utxo_size_inc",
            "total_size",
            "total_weight",
            "swtotal_size",
            "swtotal_weight",
            "total_out",
            "minfee",
            "maxfee",
            "medianfee",
            "avgfee",
            "minfeerate",
            "maxfeerate",
            "medianfeerate",
            "avgfeerate",
            "mintxsize",
            "maxtxsize",
            "mediantxsize",
            "avgtxsize",
        })

        print(stats)
        assert_equal(stats[0]['height'], start_height)
        assert_equal(stats[max_stat_pos]['height'], start_height + max_stat_pos)

        assert_equal(stats[0]['txs'], 1)
        assert_equal(stats[0]['swtxs'], 0)
        assert_equal(stats[0]['ins'], 0)
        assert_equal(stats[0]['outs'], 2)
        assert_equal(stats[0]['totalfee'], 0)
        assert_equal(stats[0]['utxo_increase'], 2)
        assert_equal(stats[0]['utxo_size_inc'], 173)
        assert_equal(stats[0]['total_size'], 0)
        assert_equal(stats[0]['total_weight'], 0)
        assert_equal(stats[0]['swtotal_size'], 0)
        assert_equal(stats[0]['swtotal_weight'], 0)
        assert_equal(stats[0]['total_out'], 0)
        assert_equal(stats[0]['minfee'], 0)
        assert_equal(stats[0]['maxfee'], 0)
        assert_equal(stats[0]['medianfee'], 0)
        assert_equal(stats[0]['avgfee'], 0)
        assert_equal(stats[0]['minfeerate'], 0)
        assert_equal(stats[0]['maxfeerate'], 0)
        assert_equal(stats[0]['medianfeerate'], 0)
        assert_equal(stats[0]['avgfeerate'], 0)
        assert_equal(stats[0]['mintxsize'], 0)
        assert_equal(stats[0]['maxtxsize'], 0)
        assert_equal(stats[0]['mediantxsize'], 0)
        assert_equal(stats[0]['avgtxsize'], 0)

        assert_equal(stats[1]['txs'], 2)
        assert_equal(stats[1]['swtxs'], 0)
        assert_equal(stats[1]['ins'], 1)
        assert_equal(stats[1]['outs'], 4)
        assert_equal(stats[1]['totalfee'], 3760)
        assert_equal(stats[1]['utxo_increase'], 3)
        assert_equal(stats[1]['utxo_size_inc'], 234)
        # assert_equal(stats[1]['total_size'], 188)
        # assert_equal(stats[1]['total_weight'], 752)
        assert_equal(stats[1]['total_out'], 4999996240)
        assert_equal(stats[1]['minfee'], 3760)
        assert_equal(stats[1]['maxfee'], 3760)
        assert_equal(stats[1]['medianfee'], 3760)
        assert_equal(stats[1]['avgfee'], 3760)
        assert_equal(stats[1]['minfeerate'], 20)
        assert_equal(stats[1]['maxfeerate'], 20)
        assert_equal(stats[1]['medianfeerate'], 20)
        assert_equal(stats[1]['avgfeerate'], 20)
        # assert_equal(stats[1]['mintxsize'], 192)
        # assert_equal(stats[1]['maxtxsize'], 192)
        # assert_equal(stats[1]['mediantxsize'], 192)
        # assert_equal(stats[1]['avgtxsize'], 192)

        assert_equal(stats[max_stat_pos]['txs'], 4)
        assert_equal(stats[max_stat_pos]['swtxs'], 2)
        assert_equal(stats[max_stat_pos]['ins'], 3)
        assert_equal(stats[max_stat_pos]['outs'], 8)
        assert_equal(stats[max_stat_pos]['totalfee'], 56880)
        assert_equal(stats[max_stat_pos]['utxo_increase'], 5)
        assert_equal(stats[max_stat_pos]['utxo_size_inc'], 380)
        # assert_equal(stats[max_stat_pos]['total_size'], 643)
        # assert_equal(stats[max_stat_pos]['total_weight'], 2572)
        assert_equal(stats[max_stat_pos]['total_out'], 9999939360)
        assert_equal(stats[max_stat_pos]['minfee'], 3320)
        assert_equal(stats[max_stat_pos]['maxfee'], 49800)
        assert_equal(stats[max_stat_pos]['medianfee'], 3760)
        assert_equal(stats[max_stat_pos]['avgfee'], 18960)
        assert_equal(stats[max_stat_pos]['minfeerate'], 20)
        # assert_equal(stats[max_stat_pos]['maxfeerate'], 300)
        assert_equal(stats[max_stat_pos]['medianfeerate'], 20)
        assert_equal(stats[max_stat_pos]['avgfeerate'], 109)
        # assert_equal(stats[max_stat_pos]['mintxsize'], 192)
        # assert_equal(stats[max_stat_pos]['maxtxsize'], 226)
        # assert_equal(stats[max_stat_pos]['mediantxsize'], 225)
        # assert_equal(stats[max_stat_pos]['avgtxsize'], 214)

        # Test invalid parameters raise the proper json exceptions
        tip = start_height + max_stat_pos
        assert_raises_rpc_error(-8, 'Target block height %d after current tip %d' % (tip+1, tip), node.getblockstats, height=tip+1)
        assert_raises_rpc_error(-8, 'Target block height %d is negative' % (-1), node.getblockstats, height=-tip-2)

        # Make sure not valid stats aren't allowed
        inv_sel_stat = 'asdfghjkl'
        inv_stats = [
            ['minfee' , inv_sel_stat],
            [inv_sel_stat, 'minfee'],
            ['minfee', inv_sel_stat, 'maxfee'],
        ]
        for inv_stat in inv_stats:
            assert_raises_rpc_error(-8, 'Invalid requested statistic %s' % inv_sel_stat, node.getblockstats, height=1, stats=inv_stat)
        # Make sure we aren't always returning inv_sel_stat as the culprit stat
        assert_raises_rpc_error(-8, 'Invalid requested statistic aaa%s' % inv_sel_stat, node.getblockstats, height=1, stats=['minfee' , 'aaa%s' % inv_sel_stat])

        # Make sure only the selected statistics are included
        some_stats = {'minfee', 'maxfee'}
        stats = node.getblockstats(height=1, stats=list(some_stats))
        # Make sure only valid stats that have been requested appear
        assert_equal(set(stats.keys()), some_stats)

if __name__ == '__main__':
    GetblockstatsTest().main()
