1. 需要继电器，继电器接入电压5v或者dcin的12v。
2. NC常闭端不连接线，常开端NO连接线。接线任意。
3. 代码在上面。使用的esp01s。
4. 使用的时候，只有GPIO0这个口是控制继电器的。其他的不控制。
5. web页面自己开GPIO0变绿1秒，就是开机了。变绿7秒就是强制关机了。
6. 注意openeluer这个不是很兼容x79.无法按钮关机，或者是没有登录的情况下， 无法按钮关机。所以，需要手动关机。
7. 需要ip判断机器是否关机，没有连接hd或者power的hd。

   8. 一段时间没有连接web端，web端刷新出页面的时间会很久，这里需要注意了。
   9. 需要两根分叉线。网上找的1根的思路，实际上可能不可用，或者他们用了其他的例如esp32c3等等。
   10. 继电器的两根线配置上power 加减两个口。
  

  ### wifi自动重连

  这里要注意wifi自动重连，代码要更新了。
  不重连，几天后掉线了就一直掉线了。
   新的代码上线了，等几天看下效果，是否还需要接着优化。
   然后可能不自己造双分线了，今天造的双分线，好像有问题，换了全新的线之后好了。。。。
暂时这么多事情。
