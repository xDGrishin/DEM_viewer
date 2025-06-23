#pragma once
#include <QtWidgets>
#include "Model.h"


class ViewerWidget :public QWidget {
	Q_OBJECT
private:
	QSize areaSize = QSize(0, 0);
	QImage* img = nullptr;
	QPainter* painter = nullptr;
	uchar* data = nullptr;

	bool drawLineActivated = false;
	QPoint drawLineBegin = QPoint(0, 0);


	Model model;
	Camera camera;

	bool drawFilledPolygons = true;
public:
	ViewerWidget(QSize imgSize, QWidget* parent = Q_NULLPTR);
	~ViewerWidget();
	void resizeWidget(QSize size);



	//
	void showModel();
	void showPoints();
	QVector<QPointF> clipPolygonToRect(const QVector<QPointF>& poly, float xmin, float xmax, float ymin, float ymax);

	//Image functions
	bool setImage(QFile& inputImg);
	QImage* getImage() { return img; };
	bool isEmpty();
	bool changeSize(int width, int height);

	void setPixel(int x, int y, uchar r, uchar g, uchar b, uchar a = 255);
	void setPixel(int x, int y, double valR, double valG, double valB, double valA = 1.);
	void setPixel(int x, int y, const QColor& color);
	bool isInside(int x, int y) { return (x >= 0 && y >= 0 && x < img->width() && y < img->height()) ? true : false; }

	//Draw functions
	void drawLine(QPoint start, QPoint end, QColor color);

	void drawCameraAxes(Camera& camera, int screenWidth, int screenHeight, float scale);
	void drawColorBar(const ColorMap& colormap);

	void setDrawLineBegin(QPoint begin) { drawLineBegin = begin; }
	QPoint getDrawLineBegin() { return drawLineBegin; }
	void setDrawLineActivated(bool state) { drawLineActivated = state; }
	bool getDrawLineActivated() { return drawLineActivated; }

	void drawPoly(QVector<QPoint> points, QColor color);
	//Get/Set functions
	uchar* getData() { return data; }
	void setDataPtr() { data = img->bits(); }
	void setPainter() { painter = new QPainter(img); }

	int getImgWidth() { return img->width(); };
	int getImgHeight() { return img->height(); };

	Model& getModel() { return model; }


	void fillPolygonScanLine(const QVector<QPointF>& polygon, const QColor& color);

	QVector3D transformModelPoint(const QVector3D& p);


	void clear();

public slots:
	void paintEvent(QPaintEvent* event) Q_DECL_OVERRIDE;

	void setModelRotationX(double angle);
	void setModelRotationY(double angle);
	void setModelRotationZ(double angle);
};



struct EdgeEntry {
	float x;        //
	float dx;       //1/m
	int dy;         //
};