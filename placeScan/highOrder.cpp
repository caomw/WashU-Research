#include "highOrder.h"

#include <iostream>
#include <list>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

static void dispHMap(const Eigen::ArrayXH &hMap,
                     const multi::Labeler::HighOrder &highOrder) {
  double average = 0;
  size_t count = 0;
  auto dataPtr = hMap.data();
  for (int i = 0; i < hMap.size(); ++i) {
    if (!(dataPtr + i)->count)
      continue;
    auto it = highOrder.find((dataPtr + i)->incident);
    if (it != highOrder.cend()) {
      assert(it->second.c != 0);
      average += it->second.w;
      ++count;
    }
  }
  average /= count;

  double sigma = 0;
  for (int i = 0; i < hMap.size(); ++i) {
    if (!(dataPtr + i)->count)
      continue;
    auto it = highOrder.find((dataPtr + i)->incident);
    if (it != highOrder.cend())
      sigma += (it->second.w - average) * (it->second.w - average);
  }
  sigma /= count - 1;
  sigma = sqrt(sigma);

  std::cout << average << "  " << sigma << std::endl;

  cv::Mat heatMap(hMap.rows(), hMap.cols(), CV_8UC3);
  fpColor.copyTo(heatMap);
  for (int j = 0; j < heatMap.rows; ++j) {
    uchar *dst = heatMap.ptr<uchar>(j);
    for (int i = 0; i < heatMap.cols; ++i) {
      if (!hMap(j, i).count)
        continue;
      auto it = highOrder.find(hMap(j, i).incident);
      if (it != highOrder.cend() && hMap(j, i).incident.size()) {
        const int gray = cv::saturate_cast<uchar>(
            255.0 * ((it->second.w - average) / sigma + 1.0) / 2.0);
        int r, g, b;
        if (gray < 128) {
          r = 0;
          g = 2 * gray;
          b = 255 - g;
        } else {
          r = 2 * (gray - 128);
          g = 255 - r;
          b = 0;
        }
        dst[3 * i + 0] = b;
        dst[3 * i + 1] = g;
        dst[3 * i + 2] = r;
      }
    }
  }

  cvNamedWindow("Preview", CV_WINDOW_NORMAL);
  cv::imshow("Preview", heatMap);
  cv::waitKey(0);
}

static void dispHMap(Eigen::ArrayXH &hMap) {
  double average = 0;
  int count = 0;
  auto const dataPtr = hMap.data();
  for (int i = 0; i < hMap.size(); ++i) {
    const double weight = (dataPtr + i)->weight;
    if (weight) {
      average += weight;
      ++count;
    }
  }
  average /= count;

  double sigma = 0;
  for (int i = 0; i < hMap.size(); ++i) {
    const double weight = (dataPtr + i)->weight;
    if (weight) {
      sigma += (weight - average) * (weight - average);
    }
  }
  sigma /= count - 1;
  sigma = std::sqrt(sigma);

  cv::Mat heatMap(hMap.rows(), hMap.cols(), CV_8UC3);
  fpColor.copyTo(heatMap);
  for (int j = 0; j < heatMap.rows; ++j) {
    uchar *dst = heatMap.ptr<uchar>(j);
    for (int i = 0; i < heatMap.cols; ++i) {
      if (hMap(j, i).weight) {
        const int gray = cv::saturate_cast<uchar>(
            255.0 * ((hMap(j, i).weight - average) / sigma + 1.0) / 2.0);
        int r, g, b;
        if (gray < 128) {
          r = 0;
          g = 2 * gray;
          b = 255 - g;
        } else {
          r = 2 * (gray - 128);
          g = 255 - r;
          b = 0;
        }
        dst[3 * i + 0] = b;
        dst[3 * i + 1] = g;
        dst[3 * i + 2] = r;
      }
    }
  }

  cvNamedWindow("Preview", CV_WINDOW_NORMAL);
  cv::imshow("Preview", heatMap);
  cv::waitKey(0);
}

static void labelNeighbours(const cv::Mat &image, const int currentLabel,
                            Eigen::RowMatrixXi &labeledImage,
                            std::list<std::pair<int, int>> &toLabel,
                            const int direction) {

  if (toLabel.empty())
    return;

  int yOffset, xOffset;
  std::tie(yOffset, xOffset) = toLabel.front();
  toLabel.pop_front();

  for (int j = -1; j <= 1; ++j) {
    if (j + yOffset < 0 || j + yOffset >= labeledImage.rows())
      continue;
    if (std::abs(j) != direction)
      continue;
    for (int i = -1; i <= 1; ++i) {
      if (std::abs(i) + std::abs(j) == 2)
        continue;
      if (std::abs(i) == direction)
        continue;
      if (i + xOffset < 0 || i + xOffset >= labeledImage.cols())
        continue;
      if (image.at<uchar>(j + yOffset, i + xOffset) != 255 &&
          labeledImage(j + yOffset, i + xOffset) == 0) {
        labeledImage(j + yOffset, i + xOffset) = currentLabel;
        toLabel.emplace_front(j + yOffset, i + xOffset);
      }
    }
  }

  labelNeighbours(image, currentLabel, labeledImage, toLabel, direction);
}

cv::Mat place::getDirections() {
  constexpr int kernelSize = 25;
  constexpr int midPoint = kernelSize / 2;
  constexpr double sum = kernelSize;
  constexpr int numLines = 1;

  std::vector<std::pair<cv::Mat, double>> kernels;

  auto line = [](int x, double slope, double yint) {
    return std::make_tuple(x, x * slope + yint);
  };
  constexpr double startSlope = 0;
  constexpr double endSlope = 1;
  constexpr double startyint = midPoint;
  constexpr double endyint = 0;
  double slope = startSlope;
  double yint = startyint;

  for (int k = 0, x, y; k < numLines; ++k) {
    int index2 = -1;
    kernels.emplace_back(cv::Mat(kernelSize, kernelSize, CV_32F, cv::Scalar(0)),
                         slope);
    int index1 = kernels.size() - 1;
    for (int i = 0; i < kernelSize; ++i) {
      std::tie(x, y) = line(i, slope, yint);
      kernels[index1].first.at<float>(y, x) = 1.0;
      if (slope != 1) {
        if (index2 == -1) {
          kernels.emplace_back(
              cv::Mat(kernelSize, kernelSize, CV_32F, cv::Scalar(0)),
              1.0 / slope);
          index2 = kernels.size() - 1;
        }
        kernels[index2].first.at<float>(x, y) = 1.0;
      }
    }
    if (slope != 0) {
      index2 = -1;
      kernels.emplace_back(
          cv::Mat(kernelSize, kernelSize, CV_32F, cv::Scalar(0)), -slope);
      index1 = kernels.size() - 1;
      for (int i = 0; i < kernelSize; ++i) {
        std::tie(x, y) = line(i, -slope, (kernelSize - 1) - yint);
        kernels[index1].first.at<float>(y, x) = 1.0;
        if (slope != 1) {
          if (index2 == -1) {
            kernels.emplace_back(
                cv::Mat(kernelSize, kernelSize, CV_32F, cv::Scalar(0)),
                1.0 / -slope);
            index2 = kernels.size() - 1;
          }
          kernels[index2].first.at<float>(x, y) = 1.0;
        }
      }
    }

    slope += (endSlope - startSlope) / (numLines - 1);
    yint += static_cast<double>(endyint - startyint) / (numLines - 1);
  }

  cv::Point anchor(-1, -1);
  double delta = 0;
  int ddepth = -1;
  cv::Mat_<cv::Vec3b> out(floorPlan.size(), cv::Vec3b(255, 255, 255));
  cv::Mat maxResponse(floorPlan.size(), CV_8UC1, cv::Scalar(255));

  for (auto &k : kernels) {
    auto &kernel = k.first;
    const double slope = k.second;
    kernel /= sum;
    cv::Mat dst;
    cv::Mat src = floorPlan.clone();

    for (int i = 0; i < 1; ++i) {
      cv::filter2D(src, dst, ddepth, kernel, anchor, delta, cv::BORDER_DEFAULT);
      dst.copyTo(src);
      // cv::threshold(src, dst, 0.2 * 255, 255, cv::THRESH_TOZERO);
      // dst.copyTo(src);
    }

    auto color = randomColor();

    for (int j = 0; j < dst.rows; ++j) {
      uchar *ptr = dst.ptr<uchar>(j);
      uchar *check = maxResponse.ptr<uchar>(j);
      for (int i = 0; i < dst.cols; ++i) {
        if (ptr[i] < check[i]) {
          out(j, i) = color;
          check[i] = ptr[i];
        }
      }
    }
  }

  if (FLAGS_visulization) {
    cvNamedWindow("Preview", CV_WINDOW_NORMAL);
    cv::imshow("Preview", out);
    cvNamedWindow("Max", CV_WINDOW_NORMAL);
    cv::imshow("Max", maxResponse);
    cv::waitKey(0);
  }

  return maxResponse;
}

void place::createHigherOrderTerms(
    const std::vector<std::vector<Eigen::MatrixXb>> &scans,
    const std::vector<std::vector<Eigen::Vector2i>> &zeroZeros,
    const std::vector<place::R2Node> &nodes,
    const std::unordered_map<int, std::unordered_set<int>> &unwantedNeighbors,
    multi::Labeler::HighOrder &highOrder) {

  const double scale = buildingScale.getScale();
  Eigen::ArrayXH hMap(floorPlan.rows, floorPlan.cols);
  for (int a = 0; a < nodes.size(); ++a) {
    auto &currentNode = nodes[a];
    auto &currentScan = scans[currentNode.color][currentNode.rotation];
    auto &zeroZero = zeroZeros[currentNode.color][currentNode.rotation];
    const int xOffset = currentNode.x - zeroZero[0],
              yOffset = currentNode.y - zeroZero[1];

    const Eigen::Vector2d center(currentNode.x, currentNode.y);

    cv::Mat_<uchar> _fp = floorPlan;
    for (int j = 0; j < currentScan.rows(); ++j) {
      if (j + yOffset < 0 || j + yOffset >= floorPlan.rows)
        continue;

      for (int i = 0; i < currentScan.cols(); ++i) {
        if (i + xOffset < 0 || i + xOffset >= floorPlan.cols)
          continue;

        if (_fp(j + yOffset, i + xOffset) != 255 &&
            localGroup(currentScan, j, i, 7)) {
          const Eigen::Vector2d ray =
              Eigen::Vector2d(i + xOffset, j + yOffset) - center;
          auto &h = hMap(j + yOffset, i + xOffset);
          if (currentNode.locked) {
            h.weight += currentNode.w;
            h.owners.push_back(a);
          } else
            h.incident.push_back(a);

          const double dist = ray.norm() / scale;
          h.distance = std::min(dist, h.distance);
        }
      }
    }
  }

  auto const data = hMap.data();

  for (int i = 0; i < hMap.size(); ++i) {
    auto &owners = (data + i)->owners;
    auto &incident = (data + i)->incident;
    for (auto &o : owners) {
      auto pair = unwantedNeighbors.find(nodes[o].id);
      if (pair == unwantedNeighbors.cend())
        continue;
      auto &exclusionSet = pair->second;
      for (auto it = incident.cbegin(); it != incident.cend();) {
        auto match = exclusionSet.find(nodes[*it].id);
        if (match != exclusionSet.cend())
          it = incident.erase(it);
        else
          ++it;
      }
    }
  }

  for (int i = 0; i < hMap.size(); ++i) {
    auto h = data + i;
    h->count = h->owners.size();
    if (h->count) {
      h->weight /= std::pow(h->count, 1.5);
      h->weight *= 3.0 - std::min(h->distance, 6.0);
    }
  }

  dispHMap(hMap);

  for (int i = 0; i < hMap.size(); ++i) {
    std::vector<int> &key = (data + i)->incident;
    const double weight = (data + i)->weight;
    if (key.size() && (data + i)->count) {
      auto it = highOrder.find(key);
      if (it != highOrder.cend()) {
        it->second.w += weight;
        ++it->second.c;
      } else
        highOrder.emplace(key, multi::Labeler::HighOrderEle(weight, 1));
    }
  }

  for (auto it = highOrder.begin(); it != highOrder.end();) {
    it->second.w /= it->second.c;
    if (std::abs(it->second.w) <= 0.001 || it->first.size() == 0)
      it = highOrder.erase(it);
    else
      ++it;
  }

  double average = 0.0, aveTerms = 0.0;
  for (auto &it : highOrder) {
    average += it.second.w;
    aveTerms += it.first.size();
  }
  average /= highOrder.size();
  aveTerms /= highOrder.size();

  double sigma = 0.0, sigTerms = 0.0;
  for (auto &it : highOrder) {
    sigma += (it.second.w - average) * (it.second.w - average);
    sigTerms += (it.first.size() - aveTerms) * (it.first.size() - aveTerms);
  }
  sigma /= (highOrder.size() - 1);
  sigma = sqrt(sigma);

  sigTerms /= (highOrder.size() - 1);
  sigTerms = sqrt(sigTerms);

  std::cout << "average: " << average << "   sigma: " << sigma << std::endl;

  for (auto &pair : highOrder)
    pair.second.w = (pair.second.w - average) / (sigma);

  for (auto it = highOrder.cbegin(); it != highOrder.cend();)
    if (it->second.w <= 0.0)
      it = highOrder.erase(it);
    else
      ++it;

  // dispHMap(hMap, highOrder);
}

double gaussianWeight(const Eigen::Array2d &pos, const Eigen::Array2d &s) {
  return std::exp(-(pos.square() / (2 * s.square())).sum());
}

void place::createHigherOrderTermsV2(
    const std::vector<std::vector<Eigen::MatrixXb>> &freeSpace,
    const std::vector<std::vector<Eigen::Vector2i>> &zeroZeros,
    const std::vector<place::node> &nodes,
    multi::Labeler::HighOrderV2 &highOrder) {

  const double scale = buildingScale.getScale();
  Eigen::ArrayXH2 hMap(floorPlan.rows, floorPlan.cols);

  for (int a = 0; a < nodes.size(); ++a) {
    auto &currentNode = nodes[a];
    auto &currentScan = freeSpace[currentNode.color][currentNode.rotation];
    auto &zeroZero = zeroZeros[currentNode.color][currentNode.rotation];
    const int xOffset = currentNode.x - zeroZero[0],
              yOffset = currentNode.y - zeroZero[1];

    constexpr double maxRange = 1.5;
    double totalWeight = 0;

    const Eigen::Array2d sigma = Eigen::Array2d(5.0, 5.0);

    for (int j = 0; j < currentScan.rows(); ++j) {
      if (j + yOffset < 0 || j + yOffset >= floorPlan.rows)
        continue;
      for (int i = 0; i < currentScan.cols(); ++i) {
        if (i + xOffset < 0 || i + xOffset >= floorPlan.cols)
          continue;

        const Eigen::Array2d pos =
            Eigen::Array2d(i - zeroZero[0], j - zeroZero[1]) / scale;

        totalWeight += currentScan(j, i) ? gaussianWeight(pos, sigma) : 0;
      }
    }

    for (int j = 0; j < currentScan.rows(); ++j) {
      if (j + yOffset < 0 || j + yOffset >= hMap.rows())
        continue;
      for (int i = 0; i < currentScan.cols(); ++i) {
        if (i + xOffset < 0 || i + xOffset >= hMap.cols())
          continue;

        if (currentScan(j, i)) {
          const Eigen::Array2d pos =
              Eigen::Array2d(i - zeroZero[0], j - zeroZero[1]) / scale;

          auto &h = hMap(j + yOffset, i + xOffset);

          h.incident.emplace_back(a);
          h.weights.emplace_back(maxRange * gaussianWeight(pos, sigma) /
                                 totalWeight);
        }
      }
    }
  }

  auto const data = hMap.data();

  for (int i = 0; i < hMap.size(); ++i) {

    auto &key = (data + i)->incident;

    if (key.size()) {

      auto &w = (data + i)->weights;
      Eigen::VectorXd weights(w.size());
      for (int i = 0; i < w.size(); ++i)
        weights[i] = w[i];

      auto it = highOrder.find(key);
      if (it != highOrder.cend())
        it->second += weights;
      else
        highOrder.emplace(key, weights);
    }
  }
}

void place::displayHighOrder(
    const multi::Labeler::HighOrder highOrder,
    const std::vector<place::R2Node> &nodes,
    const std::vector<std::vector<Eigen::MatrixXb>> &scans,
    const std::vector<std::vector<Eigen::Vector2i>> &zeroZeros) {
  for (auto &it : highOrder) {
    auto &key = it.first;
    cv::Mat output(fpColor.rows, fpColor.cols, CV_8UC3);
    fpColor.copyTo(output);
    cv::Mat_<cv::Vec3b> _output = output;
    for (auto &i : key) {
      const place::node &nodeA = nodes[i];

      auto &aScan = scans[nodeA.color][nodeA.rotation];

      auto &zeroZeroA = zeroZeros[nodeA.color][nodeA.rotation];

      int yOffset = nodeA.y - zeroZeroA[1];
      int xOffset = nodeA.x - zeroZeroA[0];
      for (int k = 0; k < aScan.cols(); ++k) {
        for (int l = 0; l < aScan.rows(); ++l) {
          if (l + yOffset < 0 || l + yOffset >= output.rows)
            continue;
          if (k + xOffset < 0 || k + xOffset >= output.cols)
            continue;

          if (aScan(l, k) != 0) {
            _output(l + yOffset, k + xOffset)[0] = 0;
            _output(l + yOffset, k + xOffset)[1] = 0;
            _output(l + yOffset, k + xOffset)[2] = 255;
          }
        }
      }
    }
    cvNamedWindow("Preview", CV_WINDOW_NORMAL);
    cv::imshow("Preview", output);
    if (!FLAGS_quietMode) {
      std::cout << it.second.w << ":  ";
      for (auto &i : key)
        std::cout << i << "_";
      std::cout << std::endl;
    }
    cv::waitKey(0);
    ~output;
  }
}
