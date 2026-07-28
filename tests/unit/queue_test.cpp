#include "common/circular_queue.hpp"
#include "test_support.hpp"

void run_queue_tests() {
    {
        CircularQueue<int, 3> queue;

        EXPECT_TRUE(queue.empty());
        EXPECT_FALSE(queue.full());
        EXPECT_EQ(queue.size(), 0U);
        EXPECT_EQ(queue.capacity(), 3U);

        EXPECT_TRUE(queue.push(10));
        EXPECT_TRUE(queue.push(20));
        EXPECT_EQ(queue.front(), 10);
        EXPECT_EQ(queue.back(), 20);
        EXPECT_EQ(queue.size(), 2U);

        EXPECT_TRUE(queue.push(30));
        EXPECT_TRUE(queue.full());
        EXPECT_EQ(queue.front(), 10);
        EXPECT_EQ(queue.back(), 30);

        // 满队列拒绝新元素，并保持原来的大小和顺序。
        EXPECT_FALSE(queue.push(40));
        EXPECT_EQ(queue.size(), 3U);
        EXPECT_EQ(queue.front(), 10);
        EXPECT_EQ(queue.back(), 30);

        EXPECT_TRUE(queue.pop());
        EXPECT_EQ(queue.front(), 20);
        EXPECT_EQ(queue.size(), 2U);

        // tail 已越过数组末端，本次插入验证下标回绕。
        EXPECT_TRUE(queue.push(40));
        EXPECT_TRUE(queue.full());
        EXPECT_EQ(queue.back(), 40);

        EXPECT_TRUE(queue.pop());
        EXPECT_EQ(queue.front(), 30);
        EXPECT_TRUE(queue.pop());
        EXPECT_EQ(queue.front(), 40);
        EXPECT_TRUE(queue.pop());
        EXPECT_TRUE(queue.empty());

        // 空队列拒绝 pop，且 size 不会发生无符号下溢。
        EXPECT_FALSE(queue.pop());
        EXPECT_TRUE(queue.empty());
        EXPECT_EQ(queue.size(), 0U);
    }

    {
        CircularQueue<int, 3> queue;
        EXPECT_TRUE(queue.push(1));
        EXPECT_TRUE(queue.push(2));
        queue.clear();

        EXPECT_TRUE(queue.empty());
        EXPECT_FALSE(queue.full());
        EXPECT_EQ(queue.size(), 0U);

        EXPECT_TRUE(queue.push(3));
        EXPECT_EQ(queue.front(), 3);
        EXPECT_EQ(queue.back(), 3);
    }

    {
        // 容量为 1 时 head 和 tail 会频繁处于同一位置，必须依赖 size 区分空和满。
        CircularQueue<int, 1> queue;
        EXPECT_TRUE(queue.empty());
        EXPECT_TRUE(queue.push(7));
        EXPECT_TRUE(queue.full());
        EXPECT_EQ(queue.front(), 7);
        EXPECT_EQ(queue.back(), 7);
        EXPECT_FALSE(queue.push(8));
        EXPECT_TRUE(queue.pop());
        EXPECT_TRUE(queue.empty());
        EXPECT_FALSE(queue.pop());
    }
}
