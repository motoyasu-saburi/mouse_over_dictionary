#include "screen_ocr.h"

ScreenOCR::ScreenOCR()
{
	tesseract_ptr = std::unique_ptr<tesseract::TessBaseAPI>(new tesseract::TessBaseAPI());

}

bool ScreenOCR::Init()
{
	// Tesseract初期化
	if (tesseract_ptr->Init(NULL, "eng")) {
		//fprintf(stderr, "Could not initialize tesseract.\n");
		return false;
	}

	// ホワイトリスト設定
	//   「-」を指定しているのに検出されない？ 一旦設定せず
	//tesseract_ptr->SetVariable("tessedit_char_whitelist", "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-+,.;:/<>()[]*'\"!? ");

	// 警告「Warning. Invalid resolution 0 dpi. Using 70 instead.」を消すため
	//tesseract_ptr->SetVariable("user_defined_dpi", "300");
	tesseract_ptr->SetVariable("user_defined_dpi", "96");

	// Page Segmentation ModeのSingle Text Lineを試してみる
	//tesseract_ptr->SetVariable("psm", "7");

	return true;
}


bool ScreenOCR::Recognize(int x, int y, int width, int height, int scale)
{

	HWND SomeWindowHandle = GetDesktopWindow();
	HDC DC = GetDC(SomeWindowHandle);

	if (DC == NULL) {
		return false;
	}

	// マウス付近の画像をキャプチャ
	try {
		//Image Img = Image(DC, x, y, width, height);
		Image Img = Image(DC, x, y, width, height, scale);
		tesseract_ptr->SetImage(Img.GetPixels(), Img.GetWidth(), Img.GetHeight(), Img.GetBytesPerPixel(), Img.GetBytesPerScanLine()); //Fixed this line..
	}
	catch (...) {
		return false;
	}

	ReleaseDC(SomeWindowHandle, DC);

	// 文字認識の向上のため、取得した画像エリアに対して前処理を行う。
	// Reference: https://stackoverflow.com/questions/28935983/preprocessing-image-for-tesseract-ocr-with-opencv
	// テキストを黒にし、残りの部分を白にする。グレースケールの場合は背景がほぼ白であることが重要。
	// ただ、テキスト部分はアンチエイリアスによって灰色であるほうが良い（場合がある）
	// これらを考慮し、マウス領域からピックアップしたエリアに対して以下の処理を実施する
	// 
	// (Option) 画像のエッジ化
	// グレースケール化
	// 二値化のためのしきい値の算出（画像のグレースケール化後がよいはず）
	// (Option)ネガポジ変換（テキストが白い場合、逆にする必要がある）
	// しきい値を基にした二値化
	//
	// しきい値の算出には、以下の３種から選ぶこと
	// http://sharky93.github.io/docs/dev/auto_examples/plot_local_otsu.html
	// http://sharky93.github.io/docs/dev/auto_examples/plot_local_equalize.html
	// https://docs.opencv.org/3.0-beta/doc/py_tutorials/py_imgproc/py_thresholding/py_thresholding.html
	// 
	// この手法を利用する場合、しきい値の算出を手法を変えることで動的に変更できるといったメリットがある。
	// テキスト取得エリアが限られる本アプリケーションでは、以下の手法についても考慮
	// 案： マウスがホバーしている領域 n pixel 分を取得・平均値（RGB ? CMYK?)を算出。それをベースにしきい値とし、自動で２階調化をおこなう。また、同時にエッジ化や、ガウスをかける処理も検討
	// この際、先に鮮鋭化を先に行うとよくなるかもしれないので、併せて検討

	// 文字認識
	tesseract_ptr->Recognize(0);
	tesseract::ResultIterator* ri = tesseract_ptr->GetIterator();
	tesseract::PageIteratorLevel level = tesseract::RIL_WORD;

	// 結果を保存
	ocr_results.clear();
	bool found = false;
	if (ri != 0) {
		do {
			ocr_result result;
			ri->BoundingBox(level, &result.x1, &result.y1, &result.x2, &result.y2);

			result.x1 = (int)(result.x1 * 100 / scale);
			result.y1 = (int)(result.y1 * 100 / scale);
			result.x2 = (int)(result.x2 * 100 / scale);
			result.y2 = (int)(result.y2 * 100 / scale);

			const char* word = ri->GetUTF8Text(level);
			if (word != NULL) {
				result.word = word;
				ocr_results.push_back(result);
				found = true;
			}
			delete[] word;
		} while (ri->Next(level));
	}

	return found;
}


void ScreenOCR::GetResults(std::vector<ocr_result>& results)
{
	results = this->ocr_results;
}
