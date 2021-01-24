# dequan
Minimal but efficient C++11 header-only library for solving Constraint Satisfaction Problems.
Simple and extensible framework for modelling constraints.

## FAQ

#### What's the license?
License is MIT.

#### What algorithms does dequan support?
dequan's aim was simplicity, it only supports basic search algorithms such as forward checking and backtracking, but it does it relatively well. 
For more advanced stuff and cutting-edge algorithms, I advise you to use an advanced package such as Google OR-Tools.

#### Why single file header?
I found OR-Tools complex and cumbersome to build. 
Single-file header allows you to just drop it in your project and make it work.

#### Can I avoid using std structures?
Yes. dequan only uses vectors (that is, resizable arrays), and you can provide your own implementation by not defining DEQUAN_USE_STDVECTOR.
For instance dequan has been tested with UE4 TArrays.