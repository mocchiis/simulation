#ifndef _DIFF_EQUATION_H_
#define _DIFF_EQUATION_H_

#include <cstdio>
#include <cstring>

class DiffEquation {
	int texWidth_, texHeight_;
	double diff_num_;
	double *fcurr_;//現在の流れ//メモリのぽいんた
	double *fnext_;//次の流れ
	double *fprev_;//前の流れ

public:
	DiffEquation()
		: texWidth_(0)
		, texHeight_(0)
		, diff_num_(0.0)
		, fcurr_(NULL)
		, fnext_(NULL)
		, fprev_(NULL) {
	}

	DiffEquation(int texWidth, int texHeight, double diff_num = 0.25)
		: texWidth_(texWidth)
		, texHeight_(texHeight)
		, diff_num_(diff_num)
		, fcurr_(NULL)
		, fnext_(NULL)
		, fprev_(NULL) {

		initmemory();
	}

	DiffEquation(const DiffEquation &diff)
		: texWidth_(0)
		, texHeight_(0)
		, diff_num_(0.0)
		, fcurr_(NULL)
		, fnext_(NULL)
		, fprev_(NULL) {
		this->operator=(diff);
	}

	//代入演算子
	DiffEquation & operator=(const DiffEquation &diff) {
		texWidth_ = diff.texWidth_;
		texHeight_ = diff.texHeight_;
		diff_num_ = diff.diff_num_;

		delete[] fcurr_;
		delete[] fprev_;

		if (diff.fcurr_ != NULL) {
			fcurr_ = new double[texWidth_ * texHeight_];
			std::memcpy(fcurr_, diff.fcurr_, sizeof(double) * texWidth_ * texHeight_);
		}
		else {
			fcurr_ = NULL;
		}

		if (diff.fnext_ != NULL) {
			fnext_ = new double[texWidth_ * texHeight_];
			std::memcpy(fnext_, diff.fnext_, sizeof(double) * texWidth_ * texHeight_);
		}
		else {
			fnext_ = NULL;
		}

		if (diff.fprev_ != NULL) {
			fprev_ = new double[texWidth_ * texHeight_];
			std::memcpy(fprev_, diff.fprev_, sizeof(double) * texWidth_ * texHeight_);
		}
		else {
			fprev_ = NULL;
		}

		return *this;
	}

	// 拡散方程式シミュレーションの初期化
	//それぞれの変数の初期化
	void initParams(int texWidth, int texHeight, double diff_num) {
		texWidth_ = texWidth;
		texHeight_ = texHeight;
		diff_num_ = diff_num;

		initmemory();
	}

	//initVAOの中：Vertex配列の作成のあとに呼び出し
	void start() {
		//memcpy:メモリfcurr_からメモリfprev_へのコピーのための最も高速なライブラリルーチン
		std::memcpy(fprev_, fcurr_, sizeof(double) * texWidth_ * texHeight_);
	}

	//animate関数内で呼び出し
	// データの更新
	void step() {
		static const int NN = 4;
		static const int dx[] = { -1, 1, 0, 0 };
		static const int dy[] = { 0, 0, -1, 1 };

		for (int y = 1; y < texHeight_ - 1; y++) {
			for (int x = 1; x < texWidth_ - 1; x++) {

				double sum = 0.0;

				for (int i = 0; i < NN; i++) {
					if (x < 0 || y < 0 || x >= texWidth_ || y >= texHeight_) {
						continue;
					}
					sum += fcurr_[(y + dy[i]) * texWidth_ + x + dx[i]] - fcurr_[y * texWidth_ + x];
				}

				fnext_[y * texWidth_ + x] = fcurr_[y * texWidth_ + x] + diff_num_ * sum;

			}
		}

		int boundary = 1;

		if (boundary = 0) {
			// 基本境界条件
			for (int x = 0; x < texWidth_; x++) {
				fnext_[0 * texWidth_ + x] = 0;
				fnext_[(texHeight_ - 1) * texWidth_ + x] = 0;
			}

			for (int y = 0; y < texHeight_; y++) {
				fnext_[y * texWidth_ + 0] = 0;
				fnext_[y * texWidth_ + (texWidth_ - 1)] = 0;
			}
		}
		else {
			// 自然境界条件
			for (int x = 0; x < texWidth_; x++) {
				fnext_[0 * texWidth_ + x] = -fnext_[1 * texWidth_ + x];
				fnext_[(texHeight_ - 1) * texWidth_ + x] = -fnext_[(texHeight_ - 2) * texWidth_ + x];
			}

			for (int y = 0; y < texHeight_; y++) {
				fnext_[y * texWidth_ + 0] = -fnext_[y * texWidth_ + 1];
				fnext_[y * texWidth_ + (texWidth_ - 1)] = -fnext_[y * texWidth_ + (texWidth_ - 2)];
			}


		}

		//memcpy:メモリfcurr_からメモリfprev_へのコピーのための最も高速なライブラリルーチン
		//メモリ先：fprev_　メモリ元：fcurr_
		std::memcpy(fprev_, fcurr_, sizeof(double) * texWidth_ * texHeight_);
		std::memcpy(fcurr_, fnext_, sizeof(double) * texWidth_ * texHeight_);
	}

	// 頂点データの初期化で使う
	void set(int x, int y, double height) {
		fcurr_[y * texWidth_ + x] = height;
	}

	//updateのなかでの頂点データの初期化
	double get(int x, int y) const {
		return fcurr_[y * texWidth_ + x];
	}

	double * const heights() const {
		return fcurr_;
	}

private:
	void initmemory() {
		//メモリの開放
		delete[] fcurr_;
		delete[] fnext_;
		delete[] fprev_;

		fcurr_ = new double[texWidth_ * texHeight_];//長方形の面積
		fnext_ = new double[texWidth_ * texHeight_];
		fprev_ = new double[texWidth_ * texHeight_];

		//memset:メモリに指定バイト数分の値をセットする
		std::memset(fcurr_, 0, sizeof(double) * texWidth_ * texHeight_);
		std::memset(fnext_, 0, sizeof(double) * texWidth_ * texHeight_);
		std::memset(fprev_, 0, sizeof(double) * texWidth_ * texHeight_);
	}


};

#endif  // _DIFF_EQUATION_H_
