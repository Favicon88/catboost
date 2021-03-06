#pragma once

#include "binarization_config.h"

#include <library/grid_creator/binarization.h>
#include <util/system/types.h>
#include <util/generic/algorithm.h>
#include <util/generic/set.h>

class IGridBuilder {
public:
    virtual ~IGridBuilder() {
    }

    virtual IGridBuilder& AddFeature(const yvector<float>& feature, ui32 borderCount) = 0;

    virtual const yvector<yvector<float>>& Borders() = 0;

    virtual yvector<float> BuildBorders(const yvector<float>& sortedFeature,
                                        ui32 borderCount) const = 0;
};

template <class T>
class IFactory;

template <>
class IFactory<IGridBuilder> {
public:
    virtual ~IFactory() {
    }

    virtual THolder<IGridBuilder> Create(EBorderSelectionType type) = 0;
};

template <class TBinarizer>
class TGridBuilderBase: public IGridBuilder {
public:
    yvector<float> BuildBorders(const yvector<float>& sortedFeature, ui32 borderCount) const override {
        yvector<float> copy(sortedFeature.begin(), sortedFeature.end());
        auto bordersSet = Binarizer.BestSplit(copy, borderCount, true);
        yvector<float> borders(bordersSet.begin(), bordersSet.end());
        Sort(borders.begin(), borders.end());
        return borders;
    }

private:
    TBinarizer Binarizer;
};

template <class TBinarizer>
class TCpuGridBuilder: public TGridBuilderBase<TBinarizer> {
public:
    IGridBuilder& AddFeature(const yvector<float>& feature,
                             ui32 borderCount) override {
        yvector<float> sortedFeature(feature.begin(), feature.end());
        Sort(sortedFeature.begin(), sortedFeature.end());
        auto borders = TGridBuilderBase<TBinarizer>::BuildBorders(sortedFeature, borderCount);
        Result.push_back(std::move(borders));
        return *this;
    }

    const yvector<yvector<float>>& Borders() override {
        return Result;
    }

private:
    yvector<yvector<float>> Result;
};

template <template <class T> class TGridBuilder>
class TGridBuilderFactory: public IFactory<IGridBuilder> {
public:
    THolder<IGridBuilder> Create(EBorderSelectionType type) override {
        THolder<IGridBuilder> builder;
        switch (type) {
            case EBorderSelectionType::UniformAndQuantiles: {
                builder.Reset(new TGridBuilder<NSplitSelection::TMedianPlusUniformBinarizer>());
                break;
            }
            case EBorderSelectionType::GreedyLogSum: {
                builder.Reset(new TGridBuilder<NSplitSelection::TMedianInBinBinarizer>());
                break;
            }
            case EBorderSelectionType::MinEntropy: {
                builder.Reset(new TGridBuilder<NSplitSelection::TMinEntropyBinarizer>());
                break;
            }
            case EBorderSelectionType::MaxLogSum: {
                builder.Reset(new TGridBuilder<NSplitSelection::TMaxSumLogBinarizer>());
                break;
            }
            case EBorderSelectionType::Median: {
                builder.Reset(new TGridBuilder<NSplitSelection::TMedianBinarizer>());
                break;
            }
            case EBorderSelectionType::Uniform: {
                builder.Reset(new TGridBuilder<NSplitSelection::TUniformBinarizer>());
                break;
            }
            default: {
                ythrow yexception() << "Invalid grid builder type!";
            }
        }
        return builder;
    }
};

class TBordersBuilder {
public:
    TBordersBuilder(IFactory<IGridBuilder>& builderFactory,
                    const yvector<float>& values)
        : BuilderFactory(builderFactory)
        , Values(values)
    {
    }

    yvector<float> operator()(const TBinarizationDescription& description) {
        auto builder = BuilderFactory.Create(description.BorderSelectionType);
        builder->AddFeature(Values, description.Discretization);
        return builder->Borders()[0];
    }

private:
    IFactory<IGridBuilder>& BuilderFactory;
    const yvector<float>& Values;
};

using TOnCpuGridBuilderFactory = TGridBuilderFactory<TCpuGridBuilder>;
