//******************************************************************************
//
// Copyright (c) 2016 Microsoft Corporation. All rights reserved.
//
// This code is licensed under the MIT License (MIT).
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
//******************************************************************************

#pragma once

#import <CloudKit/CloudKitExport.h>
#import <CloudKit/CKDatabaseOperation.h>

@class NSArray;
@class NSDictionary;
@class NSError;

CLOUDKIT_EXPORT_CLASS
@interface CKFetchRecordZonesOperation : CKDatabaseOperation
- (instancetype)initWithRecordZoneIDs:(NSArray*)zoneIDs STUB_METHOD;
+ (instancetype)fetchAllRecordZonesOperation STUB_METHOD;
@property (copy, nonatomic) NSArray* recordZoneIDs STUB_PROPERTY;
@property (copy, nonatomic, nullable) void (^fetchRecordZonesCompletionBlock)(NSDictionary*, NSError*) STUB_PROPERTY;
@end