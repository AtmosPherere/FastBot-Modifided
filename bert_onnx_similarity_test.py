#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
使用ONNX Runtime的BERT相似度计算测试脚本
更接近C++实现
"""

import sys
import json
import numpy as np
import onnxruntime as ort
import re
import os

class WordPieceTokenizer:
    """模拟C++中的WordPiece分词器"""
    
    def __init__(self, vocab_path):
        self.vocab = {}
        self.load_vocab(vocab_path)
    
    def load_vocab(self, vocab_path):
        """加载词汇表"""
        try:
            with open(vocab_path, 'r', encoding='utf-8') as f:
                for idx, line in enumerate(f):
                    token = line.strip()
                    self.vocab[token] = idx
            print(f"加载词汇表成功，共 {len(self.vocab)} 个词汇")
        except Exception as e:
            print(f"加载词汇表失败: {e}")
            # 创建默认词汇表
            self.vocab = {"[UNK]": 0, "[CLS]": 1, "[SEP]": 2, "[PAD]": 3}
    
    def is_chinese(self, c):
        """判断是否为中文字符"""
        return (c >= 0x4E00 and c <= 0x9FFF) or                (c >= 0x3400 and c <= 0x4DBF) or                (c >= 0x20000 and c <= 0x2A6DF)
    
    def clean_token(self, token):
        """清理token"""
        token = token.strip()
        if not any(self.is_chinese(ord(c)) for c in token):
            token = token.lower()
        return token
    
    def basic_tokenize(self, text):
        """基础分词"""
        if not text:
            return []
        
        # 在中文字符周围添加空格
        result = []
        i = 0
        while i < len(text):
            if self.is_chinese(ord(text[i])):
                result.append(text[i])
            else:
                # 处理非中文字符
                word = ""
                while i < len(text) and not self.is_chinese(ord(text[i])):
                    if text[i].isspace() or text[i] in ".,!?;:":
                        if word:
                            result.append(word)
                            word = ""
                    else:
                        word += text[i]
                    i += 1
                if word:
                    result.append(word)
                i -= 1
            i += 1
        
        return result
    
    def word_piece_tokenize(self, token):
        """WordPiece分词"""
        if not token:
            return []
        
        token = self.clean_token(token)
        
        if token in self.vocab:
            return [token]
        
        # 尝试子词分割
        subwords = []
        start = 0
        
        while start < len(token):
            end = len(token)
            found = False
            
            while start < end:
                subword = token[start:end]
                if start > 0:
                    subword = "##" + subword
                
                if subword in self.vocab:
                    subwords.append(subword)
                    start = end
                    found = True
                    break
                end -= 1
            
            if not found:
                subwords.append("[UNK]")
                break
        
        return subwords
    
    def tokenize(self, text):
        """完整的分词流程"""
        if not text:
            return []
        
        tokens = self.basic_tokenize(text)
        word_piece_tokens = []
        
        for token in tokens:
            word_piece_tokens.extend(self.word_piece_tokenize(token))
        
        return word_piece_tokens
    
    def convert_tokens_to_ids(self, tokens):
        """将tokens转换为IDs"""
        ids = []
        for token in tokens:
            if token in self.vocab:
                ids.append(self.vocab[token])
            else:
                ids.append(self.vocab.get("[UNK]", 0))
        return ids

class ONNXBertSimilarity:
    """ONNX BERT相似度计算器"""
    
    def __init__(self, model_path, vocab_path):
        self.session = None
        self.tokenizer = None
        self.load_model(model_path, vocab_path)
    
    def load_model(self, model_path, vocab_path):
        """加载ONNX模型和分词器"""
        try:
            # 加载ONNX模型
            self.session = ort.InferenceSession(model_path)
            print(f"ONNX模型加载成功: {model_path}")
            
            # 加载分词器
            self.tokenizer = WordPieceTokenizer(vocab_path)
            
        except Exception as e:
            print(f"加载模型失败: {e}")
            self.session = None
            self.tokenizer = None
    
    def calculate_similarity(self, text1, text2):
        """计算两个文本的相似度"""
        if not self.session or not self.tokenizer:
            return 0.0
        
        if not text1 or not text2:
            return 0.0
        
        try:
            # 分词
            tokens1 = self.tokenizer.tokenize(text1)
            tokens2 = self.tokenizer.tokenize(text2)
            
            # 添加特殊标记
            tokens1 = ["[CLS]"] + tokens1 + ["[SEP]"]
            tokens2 = ["[CLS]"] + tokens2 + ["[SEP]"]
            
            # 转换为IDs
            ids1 = self.tokenizer.convert_tokens_to_ids(tokens1)
            ids2 = self.tokenizer.convert_tokens_to_ids(tokens2)
            
            # 截断到最大长度
            max_length = 128
            ids1 = ids1[:max_length]
            ids2 = ids2[:max_length]
            
            # 创建attention mask
            attention_mask1 = [1] * len(ids1)
            attention_mask2 = [1] * len(ids2)
            
            # 填充到相同长度
            pad_length = max(len(ids1), len(ids2))
            ids1 += [0] * (pad_length - len(ids1))
            ids2 += [0] * (pad_length - len(ids2))
            attention_mask1 += [0] * (pad_length - len(attention_mask1))
            attention_mask2 += [0] * (pad_length - len(attention_mask2))
            
            # 转换为numpy数组
            input_ids1 = np.array([ids1], dtype=np.int64)
            input_ids2 = np.array([ids2], dtype=np.int64)
            attention_mask1 = np.array([attention_mask1], dtype=np.int64)
            attention_mask2 = np.array([attention_mask2], dtype=np.int64)
            
            # 运行模型
            outputs1 = self.session.run(
                ["last_hidden_state", "pooler_output"],
                {
                    "input_ids": input_ids1,
                    "attention_mask": attention_mask1,
                    "token_type_ids": np.zeros_like(input_ids1, dtype=np.int64)
                }
            )
            
            outputs2 = self.session.run(
                ["last_hidden_state", "pooler_output"],
                {
                    "input_ids": input_ids2,
                    "attention_mask": attention_mask2,
                    "token_type_ids": np.zeros_like(input_ids2, dtype=np.int64)
                }
            )
            
            # 使用pooler_output计算相似度
            embedding1 = outputs1[1][0]  # pooler_output
            embedding2 = outputs2[1][0]  # pooler_output
            
            # 计算余弦相似度
            similarity = np.dot(embedding1, embedding2) / (
                np.linalg.norm(embedding1) * np.linalg.norm(embedding2)
            )
            
            return float(similarity)
            
        except Exception as e:
            print(f"计算相似度时出错: {e}")
            return 0.0

def main():
    if len(sys.argv) != 2:
        print("用法: python3 bert_onnx_similarity_test.py <test_cases.json>")
        sys.exit(1)
    
    test_file = sys.argv[1]
    
    # 加载测试用例
    with open(test_file, 'r', encoding='utf-8') as f:
        test_cases = json.load(f)
    
    # 模型路径
    model_path = "native/desc/reuse/models/bert-base-multilingual-cased.onnx"
    vocab_path = "native/desc/reuse/models/vocab.txt"
    
    # 检查文件是否存在
    if not os.path.exists(model_path):
        print(f"ONNX模型文件不存在: {model_path}")
        sys.exit(1)
    
    if not os.path.exists(vocab_path):
        print(f"词汇表文件不存在: {vocab_path}")
        sys.exit(1)
    
    # 加载ONNX BERT模型
    print("正在加载ONNX BERT模型...")
    bert_similarity = ONNXBertSimilarity(model_path, vocab_path)
    
    if bert_similarity.session is None:
        print("无法加载ONNX BERT模型，退出")
        sys.exit(1)
    
    print("ONNX BERT模型加载成功！")
    print()
    
    # 运行测试
    results = []
    for i, case in enumerate(test_cases, 1):
        print(f"测试 {i}: {case['name']}")
        print(f"  Activity1: {case['activity1']}")
        print(f"  Activity2: {case['activity2']}")
        print(f"  预期结果: {case['expected']}")
        
        similarity = bert_similarity.calculate_similarity(case['activity1'], case['activity2'])
        
        print(f"  实际相似度: {similarity:.6f}")
        
        # 判断结果是否合理
        if case['name'] == "完全不同的activity":
            is_reasonable = similarity < 0.5
        elif case['name'] == "相似但不同的activity":
            is_reasonable = 0.3 < similarity < 0.8
        elif case['name'] == "相同activity":
            is_reasonable = similarity > 0.9
        elif case['name'] == "空字符串测试":
            is_reasonable = similarity < 0.3
        elif case['name'] == "中文activity名称":
            is_reasonable = 0.3 < similarity < 0.8
        elif case['name'] == "英文vs中文":
            is_reasonable = 0.3 < similarity < 0.8
        else:
            is_reasonable = True
        
        status = "✅ 合理" if is_reasonable else "❌ 不合理"
        print(f"  结果评估: {status}")
        
        results.append({
            'name': case['name'],
            'similarity': similarity,
            'is_reasonable': is_reasonable
        })
        
        print()
    
    # 总结
    print("=" * 60)
    print("测试总结")
    print("=" * 60)
    
    reasonable_count = sum(1 for r in results if r['is_reasonable'])
    total_count = len(results)
    
    print(f"总测试数: {total_count}")
    print(f"合理结果数: {reasonable_count}")
    print(f"不合理结果数: {total_count - reasonable_count}")
    print(f"成功率: {reasonable_count/total_count*100:.1f}%")
    
    if reasonable_count == total_count:
        print("🎉 所有测试都通过！ONNX BERT模型工作正常")
    else:
        print("⚠️  部分测试未通过，需要进一步调试")
        
        # 显示有问题的测试
        print("
有问题的测试:")
        for result in results:
            if not result['is_reasonable']:
                print(f"  - {result['name']}: 相似度 {result['similarity']:.6f}")

if __name__ == "__main__":
    main()
