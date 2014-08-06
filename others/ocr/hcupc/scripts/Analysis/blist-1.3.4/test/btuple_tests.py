# Based on Python's tuple_tests.py, licensed under the Python License
# Agreement

import unittest
import sys
from . import seq_tests
import blist
import random
import gc
from _btuple import btuple
import gc

class bTupleTest(seq_tests.CommonTest):
    type2test = btuple

    def test_constructors(self):
        super(bTupleTest, self).test_len()
        # calling built-in types without argument must return empty
        self.assertEqual(tuple(), ())
        t0_3 = (0, 1, 2, 3)
        t0_3_bis = tuple(t0_3)
        self.assert_(t0_3 is t0_3_bis)
        self.assertEqual(tuple([]), ())
        self.assertEqual(tuple([0, 1, 2, 3]), (0, 1, 2, 3))
        self.assertEqual(tuple(''), ())
        self.assertEqual(tuple('spam'), ('s', 'p', 'a', 'm'))

    def test_truth(self):
        super(bTupleTest, self).test_truth()
        self.assert_(not ())
        self.assert_((42, ))

    def test_len(self):
        super(bTupleTest, self).test_len()
        self.assertEqual(len(()), 0)
        self.assertEqual(len((0,)), 1)
        self.assertEqual(len((0, 1, 2)), 3)

    def test_iadd(self):
        super(bTupleTest, self).test_iadd()
        u = (0, 1)
        u2 = u
        u += (2, 3)
        self.assert_(u is not u2)

    def test_imul(self):
        super(bTupleTest, self).test_imul()
        u = (0, 1)
        u2 = u
        u *= 3
        self.assert_(u is not u2)

    def test_tupleresizebug(self):
        # Check that a specific bug in _PyTuple_Resize() is squashed.
        def f():
            for i in range(1000):
                yield i
        self.assertEqual(list(tuple(f())), list(range(1000)))

    def test_hash(self):
        # See SF bug 942952:  Weakness in tuple hash
        # The hash should:
        #      be non-commutative
        #      should spread-out closely spaced values
        #      should not exhibit cancellation in tuples like (x,(x,y))
        #      should be distinct from element hashes:  hash(x)!=hash((x,))
        # This test exercises those cases.
        # For a pure random hash and N=50, the expected number of occupied
        #      buckets when tossing 252,600 balls into 2**32 buckets
        #      is 252,592.6, or about 7.4 expected collisions.  The
        #      standard deviation is 2.73.  On a box with 64-bit hash
        #      codes, no collisions are expected.  Here we accept no
        #      more than 15 collisions.  Any worse and the hash function
        #      is sorely suspect.

        N=50
        base = list(range(N))
        xp = [(i, j) for i in base for j in base]
        inps = base + [(i, j) for i in base for j in xp] + \
                     [(i, j) for i in xp for j in base] + xp + list(zip(base))
        collisions = len(inps) - len(set(map(hash, inps)))
        self.assert_(collisions <= 15)

    def test_repr(self):
        l0 = btuple()
        l2 = btuple((0, 1, 2))
        a0 = self.type2test(l0)
        a2 = self.type2test(l2)

        self.assertEqual(str(a0), repr(l0))
        self.assertEqual(str(a2), repr(l2))
        self.assertEqual(repr(a0), "btuple(())")
        self.assertEqual(repr(a2), "btuple((0, 1, 2))")

    def _not_tracked(self, t):
        if sys.version_info[0] < 3:
            return
        else: # pragma: no cover
            # Nested tuples can take several collections to untrack
            gc.collect()
            gc.collect()
            self.assertFalse(gc.is_tracked(t), t)

    def _tracked(self, t):
        if sys.version_info[0] < 3:
            return
        else: # pragma: no cover
            self.assertTrue(gc.is_tracked(t), t)
            gc.collect()
            gc.collect()
            self.assertTrue(gc.is_tracked(t), t)

    def test_track_literals(self):
        # Test GC-optimization of tuple literals
        x, y, z = 1.5, "a", []

        self._not_tracked(())
        self._not_tracked((1,))
        self._not_tracked((1, 2))
        self._not_tracked((1, 2, "a"))
        self._not_tracked((1, 2, (None, True, False, ()), int))
        self._not_tracked((object(),))
        self._not_tracked(((1, x), y, (2, 3)))

        # Tuples with mutable elements are always tracked, even if those
        # elements are not tracked right now.
        self._tracked(([],))
        self._tracked(([1],))
        self._tracked(({},))
        self._tracked((set(),))
        self._tracked((x, y, z))

    def check_track_dynamic(self, tp, always_track):
        x, y, z = 1.5, "a", []

        check = self._tracked if always_track else self._not_tracked
        check(tp())
        check(tp([]))
        check(tp(set()))
        check(tp([1, x, y]))
        check(tp(obj for obj in [1, x, y]))
        check(tp(set([1, x, y])))
        check(tp(tuple([obj]) for obj in [1, x, y]))
        check(tuple(tp([obj]) for obj in [1, x, y]))

        self._tracked(tp([z]))
        self._tracked(tp([[x, y]]))
        self._tracked(tp([{x: y}]))
        self._tracked(tp(obj for obj in [x, y, z]))
        self._tracked(tp(tuple([obj]) for obj in [x, y, z]))
        self._tracked(tuple(tp([obj]) for obj in [x, y, z]))

    def test_track_dynamic(self):
        # Test GC-optimization of dynamically constructed tuples.
        self.check_track_dynamic(tuple, False)

    def test_track_subtypes(self):
        # Tuple subtypes must always be tracked
        class MyTuple(tuple):
            pass
        self.check_track_dynamic(MyTuple, True)
