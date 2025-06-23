#include   "ViewerWidget.h"
#include "Model.h"

ViewerWidget::ViewerWidget(QSize imgSize, QWidget* parent)
	: QWidget(parent)
{
	setAttribute(Qt::WA_StaticContents);
	setMouseTracking(true);
	if (imgSize != QSize(0, 0)) {
		img = new QImage(imgSize, QImage::Format_ARGB32);
		img->fill(Qt::white);
		resizeWidget(img->size());
		setPainter();
		setDataPtr();
	}
}
ViewerWidget::~ViewerWidget()
{
	delete painter;
	delete img;
}
void ViewerWidget::resizeWidget(QSize size)
{
	this->resize(size);
	this->setMinimumSize(size);
	this->setMaximumSize(size);
}


void ViewerWidget::showModel()
{
	int w = img->width();
	int h = img->height();

	std::vector<float> zBuffer(w * h, std::numeric_limits<float>::infinity());
	const float zScale = 1000.0f;
	const float margin = 20.0f;

	const QVector<Point>& allPoints = model.getPoints();
	const QVector<QVector<Point*>>& polygons = model.getPolygons();

	//Transformujem a premietam points
	QVector<QVector3D> cameraPoints;
	cameraPoints.reserve(allPoints.size());
	for (const Point& pt : allPoints) {
		/*QVector3D transformed = camera.transform(QVector3D(pt.x, pt.y, pt.z / zScale));
		cameraPoints.append(camera.project(transformed));*/

		QVector3D modelPoint(pt.x, pt.y, pt.z/model.getModelScale());
		QVector3D transformedModel = transformModelPoint(modelPoint);  
		QVector3D cameraSpace = camera.transform(transformedModel);   
		cameraPoints.append(camera.project(cameraSpace));
	}

	//model boundaries
	float minX = cameraPoints[0].x(), maxX = cameraPoints[0].x();
	float minY = cameraPoints[0].y(), maxY = cameraPoints[0].y();
	for (const QVector3D& pt : cameraPoints) {
		minX = std::min(minX, pt.x());
		maxX = std::max(maxX, pt.x());
		minY = std::min(minY, pt.y());
		maxY = std::max(maxY, pt.y());
	}
	//Center and scale the model
	float centerX = (minX + maxX) / 2.0f;
	float centerY = (minY + maxY) / 2.0f;
	float scaleX = (w - 2 * margin) / (maxX - minX);
	float scaleY = (h - 2 * margin) / (maxY - minY);
	float scale = std::min(scaleX, scaleY);

	//COLORMAP
	ColorMap colormap;
	colormap.addPoint(0.0f, QColor(0, 0, 128));   //blue 
	colormap.addPoint(0.3f, QColor(0, 255, 0));   //green
	colormap.addPoint(0.6f, QColor(255, 255, 0)); //yellow
	colormap.addPoint(1.0f, QColor(255, 0, 0));   //red


	drawColorBar(colormap);//COLORMAP


	//draw poly
	for (const auto& poly : model.getPolygons())
	{
		QVector<QPoint> screenPoly;
		if (poly.size() < 3) continue;

		//Base color by height
		//priemerna vyska poly -> normalizovana v model.normalizeZ()
		//z farebnej mapy vyberie farba
		float avgZ = 0;
		QVector3D center(0, 0, 0);
		for (const Point* p : poly) {
			avgZ += p->z;
			center += QVector3D(p->x, p->y, p->z);
		}
		avgZ /= poly.size();
		center /= poly.size();

		float normZ = model.normalizeZ(avgZ);
		QColor baseColor = colormap.getColor(normZ);

		//normal light
		QVector3D normal = model.computeNormal(poly);
		QVector3D toLight = (camera.getLightPosition() - center).normalized();

		float diffuse = std::max(0.0f, QVector3D::dotProduct(normal, toLight));
		diffuse = std::clamp(diffuse, 0.35f, 1.0f);

		QColor litColor = QColor(
			std::clamp(int(baseColor.red() * diffuse), 0, 255),
			std::clamp(int(baseColor.green() * diffuse), 0, 255),
			std::clamp(int(baseColor.blue() * diffuse), 0, 255)
		);

		


		//projection coord are centered and scaled
		for (const Point* p : poly)
		{
			int index = p - &model.getPoints()[0];
			if (index < 0 || index >= cameraPoints.size()) continue;

			const QVector3D& pt = cameraPoints[index];
			float x = (pt.x() - centerX) * scale + w / 2.0f;
			float y = (centerY - pt.y()) * scale + h / 2.0f;
			screenPoly.append(QPoint(int(x), int(y)));
		}

		if (screenPoly.size() >= 3) {
			if (drawFilledPolygons) {
				QVector<QPointF> screenPolyF;
				for (const QPoint& pt : screenPoly)
					screenPolyF.append(QPointF(pt));

				fillPolygonScanLine(screenPolyF, litColor);
			}
			else {
				//edges
				for (int i = 0; i < screenPoly.size(); ++i) {
					const QPoint& p1 = screenPoly[i];
					const QPoint& p2 = screenPoly[(i + 1) % screenPoly.size()];
					drawLine(p1, p2, litColor);
				}
			}
		}
	
	}
}

void ViewerWidget::showPoints()
{
	int w = img->width();
	int h = img->height();

	
	QVector<QPointF> screenPoints = camera.toScreenCoordinates(model.getPoints(), w, h, 20.0f);

	for (const QPointF& pt : screenPoints) {
		setPixel((int)pt.x(), (int)pt.y(), Qt::blue);
	}
}

QVector<QPointF> ViewerWidget::clipPolygonToRect(const QVector<QPointF>& poly, float xmin, float xmax, float ymin, float ymax)
{
	auto clip = [](const QVector<QPointF>& input, std::function<bool(const QPointF&)> inside,
		std::function<QPointF(const QPointF&, const QPointF&)> intersect) -> QVector<QPointF>
		{
			QVector<QPointF> output;
			if (input.isEmpty()) return output;

			QPointF S = input.last();

			for (const QPointF& P : input)
			{
				bool S_in = inside(S);
				bool P_in = inside(P);

				if (P_in)
				{
					if (!S_in)
						output.append(intersect(S, P));
					output.append(P);
				}
				else if (S_in)
				{
					output.append(intersect(S, P));
				}
				S = P;
			}

			return output;
		};

	QVector<QPointF> result = poly;

	//left (x >= xmin)
	result = clip(result,
		[xmin](const QPointF& p) { return p.x() >= xmin; },
		[xmin](const QPointF& a, const QPointF& b) {
			float t = (xmin - a.x()) / (b.x() - a.x());
			float y = a.y() + t * (b.y() - a.y());
			return QPointF(xmin, y);
		});

	//right (x <= xmax)
	result = clip(result,
		[xmax](const QPointF& p) { return p.x() <= xmax; },
		[xmax](const QPointF& a, const QPointF& b) {
			float t = (xmax - a.x()) / (b.x() - a.x());
			float y = a.y() + t * (b.y() - a.y());
			return QPointF(xmax, y);
		});

	//up (y >= ymin)
	result = clip(result,
		[ymin](const QPointF& p) { return p.y() >= ymin; },
		[ymin](const QPointF& a, const QPointF& b) {
			float t = (ymin - a.y()) / (b.y() - a.y());
			float x = a.x() + t * (b.x() - a.x());
			return QPointF(x, ymin);
		});

	//below (y <= ymax)
	result = clip(result,
		[ymax](const QPointF& p) { return p.y() <= ymax; },
		[ymax](const QPointF& a, const QPointF& b) {
			float t = (ymax - a.y()) / (b.y() - a.y());
			float x = a.x() + t * (b.x() - a.x());
			return QPointF(x, ymax);
		});

	return result;
}


//Image functions
bool ViewerWidget::setImage(QFile& file)
{
	
	QTextStream in(&file);
	
	while (!in.atEnd()) {
		QString line = in.readLine().trimmed();
		if (line.isEmpty()) continue;

		QStringList parts = line.split(QRegularExpression("\\s+"));
		if (parts.size() != 3) {
			qWarning() << "Bad line" << line;
			continue;
		}

		bool ok1, ok2, ok3;
		double x = parts[0].toDouble(&ok1);
		double y = parts[1].toDouble(&ok2);
		double z = parts[2].toDouble(&ok3);

		if (ok1 && ok2 && ok3) {
			Point point{ x,y,z };
			model.getPoints().append(point);
		}
		
	}

	update();

	qDebug() << "File loaded";
	//model.generateTestGrid(5, 5, 1.0); // TESTTTTT
	model.setupModel();
	clear();
	showModel();
	
	//drawCameraAxes(camera,img->width(), img->height(), 100);
	//showPoints(); // TEST
	return true;

}
bool ViewerWidget::isEmpty()
{
	if (img == nullptr) {
		return true;
	}

	if (img->size() == QSize(0, 0)) {
		return true;
	}
	return false;
}

bool ViewerWidget::changeSize(int width, int height)
{
	QSize newSize(width, height);

	if (newSize != QSize(0, 0)) {
		if (img != nullptr) {
			delete painter;
			delete img;
		}

		img = new QImage(newSize, QImage::Format_ARGB32);
		if (!img) {
			return false;
		}
		img->fill(Qt::white);
		resizeWidget(img->size());
		setPainter();
		setDataPtr();
		update();
	}

	return true;
}

void ViewerWidget::setPixel(int x, int y, uchar r, uchar g, uchar b, uchar a)
{
	r = r > 255 ? 255 : (r < 0 ? 0 : r);
	g = g > 255 ? 255 : (g < 0 ? 0 : g);
	b = b > 255 ? 255 : (b < 0 ? 0 : b);
	a = a > 255 ? 255 : (a < 0 ? 0 : a);

	size_t startbyte = y * img->bytesPerLine() + x * 4;
	data[startbyte] = b;
	data[startbyte + 1] = g;
	data[startbyte + 2] = r;
	data[startbyte + 3] = a;
}
void ViewerWidget::setPixel(int x, int y, double valR, double valG, double valB, double valA)
{
	valR = valR > 1 ? 1 : (valR < 0 ? 0 : valR);
	valG = valG > 1 ? 1 : (valG < 0 ? 0 : valG);
	valB = valB > 1 ? 1 : (valB < 0 ? 0 : valB);
	valA = valA > 1 ? 1 : (valA < 0 ? 0 : valA);

	size_t startbyte = y * img->bytesPerLine() + x * 4;
	data[startbyte] = static_cast<uchar>(255 * valB);
	data[startbyte + 1] = static_cast<uchar>(255 * valG);
	data[startbyte + 2] = static_cast<uchar>(255 * valR);
	data[startbyte + 3] = static_cast<uchar>(255 * valA);
}
void ViewerWidget::setPixel(int x, int y, const QColor& color)
{
	if (color.isValid()) {
		size_t startbyte = y * img->bytesPerLine() + x * 4;

		data[startbyte] = color.blue();
		data[startbyte + 1] = color.green();
		data[startbyte + 2] = color.red();
		data[startbyte + 3] = color.alpha();
	}
}

//Draw functions
void ViewerWidget::drawLine(QPoint start, QPoint end, QColor color)
{
	int deltaY = (end.y() - start.y());
	int deltaX = (end.x() - start.x());

	int x, y, x1, y1, x2, y2, k1, k2, p1;
	double m;

	if (start.x() == end.x()) {  // vert
		int y1 = std::min(start.y(), end.y());
		int y2 = std::max(start.y(), end.y());
		for (int y = y1; y <= y2; ++y) {
			setPixel(start.x(), y, color);
		}
		update();
		return;
	}
	if (start.y() == end.y()) {  // horiz
		int x1 = std::min(start.x(), end.x());
		int x2 = std::max(start.x(), end.x());
		for (int x = x1; x <= x2; ++x) {
			setPixel(x, start.y(), color);
		}
		update();
		return;
	}


	if (deltaX != 0) {
		m = static_cast<double>(deltaY) / static_cast<double>(deltaX);
	}
	else {
		m = static_cast<double>(deltaY) / DBL_MAX;
	}
	if (((m > 0) && (m < 1)) || ((m > -1) && (m < 0)))
	{
		//qDebug() << "X";
		//x
		//BeginEndPoints
		if (end.x() > start.x()) {
			x1 = start.x();
			x2 = end.x();
			y1 = start.y();
			y2 = end.y();
		}
		else {
			x1 = end.x();
			x2 = start.x();
			y1 = end.y();
			y2 = start.y();
			deltaX *= -1;
			deltaY *= -1;
		}
		//Algorithm
		if ((m > 0) && (m < 1)) {
			//qDebug() << "X1";
			k1 = 2 * deltaY;
			k2 = 2 * deltaY - 2 * deltaX;
			p1 = 2 * deltaY - deltaX;
			x = x1;
			y = y1;
			setPixel(x, y, color);
			while (x < x2)
			{
				x++;
				if (p1 > 0) {
					y++;
					p1 += k2;
				}
				else {
					p1 += k1;
				}
				setPixel(x, y, color);
			}
		}
		else {
			//qDebug() << "X2";
			k1 = 2 * deltaY;
			k2 = 2 * deltaY + 2 * deltaX;
			p1 = 2 * deltaY + deltaX;
			x = x1;
			y = y1;
			setPixel(x, y, color);
			while (x < x2)
			{
				x++;
				if (p1 < 0) {
					y--;
					p1 += k2;
				}
				else {
					p1 += k1;
				}
				setPixel(x, y, color);
			}
		}
	}
	else {
		//qDebug() << "Y";
		//y
		//BeginEndPoints
		if (end.y() > start.y()) {
			x1 = start.x();
			x2 = end.x();
			y1 = start.y();
			y2 = end.y();
		}
		else {
			x1 = end.x();
			x2 = start.x();
			y1 = end.y();
			y2 = start.y();
			deltaX *= -1;
			deltaY *= -1;
		}
		//Algorithm
		if (m > 0) {
			k1 = 2 * deltaX;
			k2 = 2 * deltaX - 2 * deltaY;
			p1 = 2 * deltaX - deltaY;
			x = x1;
			y = y1;
			setPixel(x, y, color);
			while (y < y2)
			{
				y++;
				if (p1 > 0) {
					x++;
					p1 += k2;
				}
				else {
					p1 += k1;
				}
				setPixel(x, y, color);
			}
		}
		else {
			k1 = 2 * deltaX;
			k2 = 2 * deltaX + 2 * deltaY;
			p1 = 2 * deltaX + deltaY;
			x = x1;
			y = y1;
			setPixel(x, y, color);
			while (y < y2)
			{
				y++;
				if (p1 < 0) {
					x--;
					p1 += k2;
				}
				else {
					p1 += k1;
				}
				setPixel(x, y, color);
			}
		}
	}

	update();
}

void ViewerWidget::drawCameraAxes(Camera& camera, int screenWidth, int screenHeight, float scale)
{
	QVector3D origin = camera.getPosition();

    QVector3D u_end = origin + camera.getU() * 10.0f;
    QVector3D v_end = origin + camera.getV() * 10.0f;
    QVector3D n_end = origin - camera.getN() * 10.0f;

    auto toScreen = [&](QVector3D pt) {
        QVector3D cam = camera.transform(pt);
        QVector3D proj = camera.project(cam);
        return proj.toPoint();
    };


    drawLine(toScreen(origin), toScreen(u_end), Qt::red);

    drawLine(toScreen(origin), toScreen(v_end), Qt::green);

    drawLine(toScreen(origin), toScreen(n_end), Qt::blue);
}

void ViewerWidget::drawColorBar(const ColorMap& colormap)
{
	int w = img->width();
	int h = img->height();

	const int barWidth = 20;
	const int barX = w - barWidth - 10;
	const int barY = 10;
	const int barHeight = h - 20;

	for (int y = 0; y < barHeight; ++y)
	{
		
		float normZ = 1.0f - float(y) / barHeight;

		QColor color = colormap.getColor(normZ);

		for (int x = 0; x < barWidth; ++x)
		{
			setPixel(barX + x, barY + y, color);
		}
	}
}

void ViewerWidget::drawPoly(QVector<QPoint> points, QColor color)
{
	for (int i = 0; i < points.size(); i++)
	{
		drawLine(points[i], points[(i + 1) % points.size()], color);
	}
}


void ViewerWidget::fillPolygonScanLine(const QVector<QPointF>& polygon, const QColor& color)
{
	if (polygon.size() < 3) return;

	//edges setup
	QVector<QVector<EdgeEntry>> edgeTable;

	int ymin = std::numeric_limits<int>::max();
	int ymax = std::numeric_limits<int>::min();

	for (int i = 0; i < polygon.size(); i++) {
		QPointF p1 = polygon[i];
		QPointF p2 = polygon[(i + 1) % polygon.size()];

		if (p1.y() == p2.y()) continue; //horiz skip
		QPointF upper = p1.y() < p2.y() ? p1 : p2;
		QPointF lower = p1.y() < p2.y() ? p2 : p1;

		int yStart = std::ceil(upper.y());
		int yEnd = std::floor(lower.y()) - 1;

		if (yStart > yEnd) continue;

		float dx = (lower.x() - upper.x()) / (lower.y() - upper.y()); //m

		while (edgeTable.size() <= yEnd)
		{
			edgeTable.append(QVector<EdgeEntry>());
		}

		EdgeEntry entry;
		entry.x = upper.x();
		entry.dx = dx;
		entry.dy = yEnd - yStart + 1;

		edgeTable[yStart].append({ entry });

		ymin = std::min(ymin, yStart);
		ymax = std::max(ymax, yEnd);
	}

	//skanline
	QVector<EdgeEntry> activeEdges;

	for (int y = ymin; y <= ymax; ++y) {
		if (y < edgeTable.size()) {
			for (EdgeEntry& e : edgeTable[y]) {
				activeEdges.append(e);
			}
		}

		for (int i = 0; i < activeEdges.size(); ) {
			if (activeEdges[i].dy <= 0)
				activeEdges.removeAt(i);
			else
				++i;
		}

		//sort by x
		std::sort(activeEdges.begin(), activeEdges.end(), [](const EdgeEntry& a, const EdgeEntry& b) {
			return a.x < b.x;
			});

		//draw
		for (int i = 0; i + 1 < activeEdges.size(); i += 2) {
			int xStart = std::ceil(activeEdges[i].x);
			int xEnd = std::floor(activeEdges[i + 1].x);

			for (int x = xStart; x <= xEnd; ++x) {
				setPixel(x, y, color);
			}
		}

		for (EdgeEntry& e : activeEdges) {
			e.x += e.dx;
			e.dy -= 1;
		}
	}
}

QVector3D ViewerWidget::transformModelPoint(const QVector3D& p)
{
	QVector3D point = p;



	QMatrix4x4 mat;
	mat.rotate(model.getModelRotation().x(), 1, 0, 0);
	mat.rotate(model.getModelRotation().y(), 0, 1, 0);
	mat.rotate(model.getModelRotation().z(), 0, 0, 1);


	mat.scale(model.getModelScale(), model.getModelScale(), model.getModelScale() * model.getZScaleFactor());


	point = mat * point;


	point += model.getModelTranslation();

	return point;
}



void ViewerWidget::clear()
{
	img->fill(Qt::white);
	update();
}



//Slots
void ViewerWidget::paintEvent(QPaintEvent* event)
{
	QPainter painter(this);
	QRect area = event->rect();
	painter.drawImage(area, *img, area);
}

void ViewerWidget::setModelRotationX(double angle)
{
	model.setModelRotation(QVector3D(angle, model.getModelRotation().y(), model.getModelRotation().z()));
	update();
	clear();
	showModel();
}
void ViewerWidget::setModelRotationY(double angle) {
	model.setModelRotation(QVector3D(model.getModelRotation().x(), angle, model.getModelRotation().z()));
	update();
	clear();
	showModel();
}
void ViewerWidget::setModelRotationZ(double angle) {
	model.setModelRotation(QVector3D(model.getModelRotation().x(), model.getModelRotation().y(), angle));
	update();
	clear();
	showModel();
}

