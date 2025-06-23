#include "Model.h"
#include <QDebug>


void Model::setupModel()
{
	edgesSetup();
	//edgesPrint();

}

void Model::printPoints()
{
	for (auto& x : getPoints())
	{
		qDebug() << x.x << x.y << x.z;
	}
}



void Model::edgesSetup()
{
	size_t i = 0, j = 0, k = 0;
	int rows = 0, cols = 1;
	bool isLastEdge = false;

	while (i + 1 < points.size())
	{
		Point& first = points[i];
		Point& second = points[i + 1];

		constexpr double EPS = 1e-6;
		
		if (std::abs(first.y - second.y) < EPS)
		{
			edges.append(std::pair<Point*, Point*>(&points[i], &points[i + 1]));
		}
		else
		{
			if (isLastEdge)
			{
				edges.append(std::pair<Point*, Point*>(&points[k], &points[i]));
				k = i;
			}
			else
			{
				isLastEdge = true;
				k = i;
				rows = i + 1;
			}

			edges.append(std::pair<Point*, Point*>(&points[j], &points[i + 1]));
			j = i + 1;
			cols++;
		}
		++i;
	}
	qDebug() << "Rows" << rows << " Cols" << cols;
	polygonsSetup(rows, cols);
}



void Model::polygonsSetup(int rows, int cols)
{
	for (size_t i = 0; i < rows - 1; ++i)
	{
		for (size_t j = 0; j < cols - 1; ++j)
		{
			size_t idx = i * cols + j;

			Point* A = &points[idx];
			Point* B = &points[idx + 1];
			Point* C = &points[idx + cols + 1];
			Point* D = &points[idx + cols];

			polygons.append({ A, B, C, D });
		}
	}
	computeZRange();
}

void Model::edgesPrint()
{
	for (auto& x : edges)
	{
		qDebug() << x.first->x << x.first->y << x.first->z << "//" << x.second->x << x.second->y << x.second->z;
	}
}


void Model::generateTestGrid(int rows, int cols, double spacing)
{
	points.clear();
	polygons.clear(); 

	
	for (int i = 0; i < rows; ++i) {
		for (int j = 0; j < cols; ++j) {
			points.append(Point(j * spacing, i * spacing, 0.0));
		}
	}

	for (int i = 0; i < rows - 1; ++i) {
		for (int j = 0; j < cols - 1; ++j) {
			int idx = i * cols + j;
			QVector<Point*> polygon;
			polygon << &points[idx]
				<< &points[idx + 1]
				<< &points[idx + cols + 1]
				<< &points[idx + cols];
			polygons.append(polygon);
		}
	}
}

void Model::computeZRange()
{
	if (points.isEmpty()) return;
	minZ = maxZ = points[0].z;
	for (const Point& p : points) {
		if (p.z < minZ) 
			minZ = p.z;
		if (p.z > maxZ) 
			maxZ = p.z;
	}
}

QVector3D Model::computeNormal(const QVector<Point*>& poly)
{
	if (poly.size() < 3) return QVector3D(0, 0, 1);

	QVector3D A(poly[0]->x, poly[0]->y, poly[0]->z);
	QVector3D B(poly[1]->x, poly[1]->y, poly[1]->z);
	QVector3D C(poly[2]->x, poly[2]->y, poly[2]->z);

	QVector3D u = B - A;
	QVector3D v = C - A;
	return QVector3D::crossProduct(u, v).normalized();
}






void Camera::cameraSetup()
{
	float zenithRad = qDegreesToRadians(angles.zenit);
	float azimuthRad = qDegreesToRadians(angles.azimut);

	//camera look
	n = QVector3D(sin(zenithRad) * cos(azimuthRad),	sin(zenithRad) * sin(azimuthRad),cos(zenithRad)).normalized();

	//not parallel
	QVector3D tmpUp = (std::abs(n.z()) > 0.99) ? QVector3D(0, 1, 0) : QVector3D(0, 0, 1);

	//right
	u = QVector3D::crossProduct(tmpUp, n).normalized();

	//up
	v = QVector3D::crossProduct(n, u).normalized();
	qDebug() << "Camera";
	qDebug() << angles.zenit << angles.azimut;
	qDebug() << n << u << v;
}

QVector3D Camera::transform(Point point)
{
	QVector3D pointVector(point.x, point.y, point.z);
	QVector3D viewVector = pointVector - position;

	float x = QVector3D::dotProduct(viewVector, v); 
	float y = QVector3D::dotProduct(viewVector, u);  
	float z = QVector3D::dotProduct(viewVector, n); 

	return QVector3D(x, y, z);
}

QVector3D Camera::transform(QVector3D point)
{
	QVector3D viewVector = point - position;

	float x = QVector3D::dotProduct(viewVector, u);
	float y = QVector3D::dotProduct(viewVector, v);
	float z = QVector3D::dotProduct(viewVector, n);

	return QVector3D(x, y, z);
}

QVector3D Camera::project(QVector3D cameraPoint)
{
	/*float x = cameraPoint.x() / scale;
	float y = cameraPoint.y() / scale;
	return QVector3D(x, y, 0);*/
	return QVector3D(cameraPoint.x(), cameraPoint.y(), 0);
}

QPointF Camera::toScreenCoordinatesCentered(QVector3D projected, int screenWidth, int screenHeight)
{
	float x = screenWidth / 2.0f + projected.x();
	float y = screenHeight / 2.0f - projected.y(); 
	return QPointF(x, y);
}

QVector<QPointF> Camera::toScreenCoordinates(QVector<Point>& points, int screenWidth, int screenHeight, float margin)
{
	if (points.isEmpty()) return {};

	//to camera system
	QVector<QVector3D> cameraPoints;
	cameraPoints.reserve(points.size());
	for (const Point& pt : points) {
		cameraPoints.append(transform(QVector3D(pt.x, pt.y, pt.z)));
	}

	//bounding box
	float minX = cameraPoints[0].x(), maxX = cameraPoints[0].x();
	float minY = cameraPoints[0].y(), maxY = cameraPoints[0].y();
	for (const QVector3D& pt : cameraPoints) {
		if (pt.x() < minX) minX = pt.x();
		if (pt.x() > maxX) maxX = pt.x();
		if (pt.y() < minY) minY = pt.y();
		if (pt.y() > maxY) maxY = pt.y();
	}

	float centerX = (minX + maxX) / 2.0f;
	float centerY = (minY + maxY) / 2.0f;
	float width = maxX - minX;
	float height = maxY - minY;

	//scale
	float scaleX = (screenWidth - 2 * margin) / width;
	float scaleY = (screenHeight - 2 * margin) / height;
	float scale = std::min(scaleX, scaleY);

	//to screenCoords
	QVector<QPointF> screenPoints;
	screenPoints.reserve(points.size());

	for (const QVector3D& pt : cameraPoints) {
		float x = (pt.x() - centerX) * scale + screenWidth / 2.0f;
		float y = (centerY - pt.y()) * scale + screenHeight / 2.0f;
		screenPoints.append(QPointF(x, y));
	}

	return screenPoints;
}


