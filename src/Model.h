#pragma once
#include <QtWidgets>
#include <QVector>
#include <QColor>

struct Point {
	Point(double _x, double _y, double _z) : x{ _x }, y{ _y }, z{ _z } {}
	double x, y, z;
	double nx = 0, ny = 0, nz = 0;
	void print() { qDebug() << x << y << z; }

};

struct Triangle {
	Point p1, p2, p3;
	QColor color;
	double avgZ;
};

struct Angles {
	double zenit; 
	double azimut;   
};

class Camera {
public:
	Camera() : position{ 0,0,200 }, angles{ 45,45 } { cameraSetup(); }
	void cameraSetup();
	QVector3D transform(Point point);
	QVector3D transform(QVector3D point);

	QVector3D project(QVector3D cameraPoint);
	QPointF toScreenCoordinatesCentered(QVector3D projected, int screenWidth, int screenHeight);
	QVector<QPointF> toScreenCoordinates(QVector<Point>& points, int screenWidth, int screenHeight, float margin);
	QVector3D getPosition() { return position; };
	QVector3D getU() { return u; }
	QVector3D getN() { return n; }
	QVector3D getV() { return v; }
	QVector3D getLightPosition() { return lightPos; };
private:
	QVector3D n, u, v;
	QVector3D position;
	Angles angles;

	QVector3D lightPos = QVector3D(0, 0, 200);
};

class Model {
public:
	void setupModel();
	void printPoints();
	void edgesSetup();
	void polygonsSetup(int rows, int cols);
	void edgesPrint();

	QVector<Point>& getPoints() { return points; }
	QVector<std::pair<Point*, Point*>>& getEdges() { return edges; }
	QVector<QVector<Point*>>& getPolygons() { return polygons; }

	void generateTestGrid(int rows, int cols, double spacing);
	void computeZRange();
	float normalizeZ(float z) {	return (z - minZ) / (maxZ - minZ);}

	QVector3D computeNormal(const QVector<Point*>& poly);

	QVector3D getModelRotation() { return modelRotation; }
	QVector3D getModelTranslation() { return modelTranslation; }
	float& getModelScale() { return modelScale; }
	float& getZScaleFactor() { return zScaleFactor; }
	void setModelRotation(QVector3D rot) { modelRotation = rot; }
	void setModelTranslation(QVector3D transl) { modelTranslation = transl; }

private:
	QVector<Point> points;
	QVector<std::pair<Point*, Point*>> edges;
	QVector<QVector<Point*>> polygons;
	double minZ = 0, maxZ = 1;


	QVector3D modelRotation = QVector3D(0, 0, 0); //X, Y, Z
	QVector3D modelTranslation = QVector3D(0, 0, 0); 
	float modelScale = 1000.0f;        
	float zScaleFactor = 1.0f;
};










struct ColorPoint {
	float x;       //[0, 1]
	QColor color;
};

class ColorMap {
public:
	void addPoint(float x, const QColor& color) {
		points.append({ x, color });
		std::sort(points.begin(), points.end(), [](const ColorPoint& a, const ColorPoint& b) {
			return a.x < b.x;
			});
	}

	QColor getColor(float x) const {
		if (points.isEmpty()) return QColor(0, 0, 0);

		if (x <= points.first().x) return points.first().color;
		if (x >= points.last().x) return points.last().color;

		for (int i = 0; i < points.size() - 1; ++i) {
			if (x >= points[i].x && x <= points[i + 1].x) {
				float t = (x - points[i].x) / (points[i + 1].x - points[i].x);
				const QColor& c1 = points[i].color;
				const QColor& c2 = points[i + 1].color;
				int r = c1.red() + t * (c2.red() - c1.red());
				int g = c1.green() + t * (c2.green() - c1.green());
				int b = c1.blue() + t * (c2.blue() - c1.blue());
				return QColor(r, g, b);
			}
		}

		return QColor(0, 0, 0);
	}

private:
	QVector<ColorPoint> points;
};